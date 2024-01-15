#include "CaptureManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHICommandList.h"
#include "SocketIOClientComponent.h"
#include "ImageUtils.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Async/ParallelFor.h"
#include "Engine/SceneCapture2D.h"

class UCameraComponent;

UCaptureManager::UCaptureManager()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UCaptureManager::BeginPlay()
{
	Super::BeginPlay();
	
	if (!ColorCapture.IsValid())
	{
		if (!ColorCapture.IsValid())
		{
			// UE_LOG(LogTemp, Error, TEXT("ColorCapture was not valid!"));
		}
		return;
	}

	SetupColorCaptureComponent(ColorCapture.Get());
	SetupSegmentationCaptureComponent(ColorCapture.Get());
}

/**
 * @brief Initializes the render targets and material
 */
void UCaptureManager::SetupColorCaptureComponent(ASceneCapture2D* ParamCapture)
{
	// scene capture component render target (stores frame that is then pulled from gpu to cpu)
	UTextureRenderTarget2D* RenderTarget2D = NewObject<UTextureRenderTarget2D>();
	RenderTarget2D->InitAutoFormat(256, 256); // some random format, got crashing otherwise
	RenderTarget2D->InitCustomFormat(ModelImageProperties.width, ModelImageProperties.height, PF_B8G8R8A8, true);
	// PF_B8G8R8A8 disables HDR which will boost storing to disk due to less image information
	RenderTarget2D->RenderTargetFormat = RTF_RGBA8;
	RenderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU
	RenderTarget2D->TargetGamma = 1.2f; // for Vulkan //GEngine->GetDisplayGamma(); // for DX11/12

	USceneCaptureComponent2D* CaptureComponent = ParamCapture->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = RenderTarget2D;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	// CaptureComponent->ShowFlags.SetTemporalAA(true);

	ParamCapture->AttachToActor(MySceneCap->GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// Set location of color capture to be same as HUDSceneCapture
	CaptureComponent->SetWorldTransform(MySceneCap->GetComponentTransform());
	// set fov angle to be same as HUDSceneCapture
	CaptureComponent->FOVAngle = MySceneCap->FOVAngle;
}

void UCaptureManager::SetupSegmentationCaptureComponent(ASceneCapture2D* ParamCapture)
{
	SpawnSegmentationCaptureComponent(ParamCapture);
	SetupColorCaptureComponent(SegmentationCapture);

	if (PostProcessMaterial)
	{
		USceneCaptureComponent2D* CaptureComponent = SegmentationCapture->GetCaptureComponent2D();
		CaptureComponent->AddOrUpdateBlendable(PostProcessMaterial);
	}
}

void UCaptureManager::SpawnSegmentationCaptureComponent(ASceneCapture2D* ParamCapture)
{
	ASceneCapture2D* newSegmentationCapture = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass());
	// Register new CaptureComponent to game
	newSegmentationCapture->GetCaptureComponent2D()->RegisterComponent();
	// Attach SegmentationCaptureComponent to match ColorCaptureComponent
	if (!IsValid(ParamCapture))
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnSegmentationCaptureComponent: ParamCapture was not valid!"));
		return;
	}
	newSegmentationCapture->AttachToActor(ParamCapture, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// Get values from "parent" ColorCaptureComponent
	newSegmentationCapture->GetCaptureComponent2D()->FOVAngle = ParamCapture->GetCaptureComponent2D()->FOVAngle;
	SegmentationCapture = newSegmentationCapture;
}

/**
 * @brief Sends request to gpu to read frame (send from gpu to cpu)
 * @param CaptureComponent 
 * @param IsSegmentation 
 */
void UCaptureManager::CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation)
{
	USceneCaptureComponent2D* ColorCaptureComponent = ColorCapture->GetCaptureComponent2D();
	USceneCaptureComponent2D* SegmentationCaptureComponent = SegmentationCapture->GetCaptureComponent2D();
	if (!IsValid(SegmentationCaptureComponent) || !IsValid(ColorCaptureComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
		return;
	}

	FTextureRenderTargetResource* renderTargetResource1 = SegmentationCaptureComponent->TextureTarget->
		GameThread_GetRenderTargetResource();

	const int32& rtx1 = SegmentationCaptureComponent->TextureTarget->SizeX;
	const int32& rty1 = SegmentationCaptureComponent->TextureTarget->SizeY;

	FTextureRenderTargetResource* renderTargetResource2 = ColorCaptureComponent->TextureTarget->
		GameThread_GetRenderTargetResource();

	FRenderRequest* renderRequest = new FRenderRequest();
	renderRequest->isPNG = IsSegmentation;

	int32 width = rtx1;
	int32 height = rty1;
	ScreenImageProperties = {width, height};

	struct FReadSurfaceContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FColor>* OutData;
		FIntRect Rect;
		FReadSurfaceDataFlags Flags;
	};
	FReadSurfaceContext readSurfaceContext1 = {
		renderTargetResource1,
		&(renderRequest->Image1),
		FIntRect(0, 0, width, height),
		FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
	};
	FReadSurfaceContext readSurfaceContext2 = {
		renderTargetResource2,
		&(renderRequest->Image2),
		FIntRect(0, 0, width, height),
		FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
	};
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[readSurfaceContext1, readSurfaceContext2](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceData(
				readSurfaceContext1.SrcRenderTarget->GetRenderTargetTexture(),
				readSurfaceContext1.Rect,
				*readSurfaceContext1.OutData,
				readSurfaceContext1.Flags
			);
			RHICmdList.ReadSurfaceData(
				readSurfaceContext2.SrcRenderTarget->GetRenderTargetTexture(),
				readSurfaceContext2.Rect,
				*readSurfaceContext2.OutData,
				readSurfaceContext2.Flags
			);
		});
	renderRequest->RenderFence.BeginFence(true);

	RenderRequestQueue.Enqueue(renderRequest);
}

TArray<FVector> UCaptureManager::GetOutlineOfStaticMesh(UStaticMesh* StaticMesh, FTransform& ComponentToWorldTransform)
{
	// Declare the map or set to store the vertex indices and counts
	TMap<int32, int32> VertexCounts;

	// Declare the arrays for the geometry data
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	// Get the section from the static mesh asset
	UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(StaticMesh, 0, 0, Vertices, Triangles, Normals, UVs,
	                                                       Tangents);

	// Convert the vertex positions to world space
	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		Vertices[i] = ComponentToWorldTransform.TransformPosition(Vertices[i]);
	}
	return Vertices;
}

FColor AddTransparentRed(FColor InColor)
{
	// Create a constant vector for the red color
	FLinearColor RedColor(1.0f, 0.0f, 0.0f, 0.5f); // 0.5f is the alpha value for transparency

	// Convert the input FColor to FLinearColor
	FLinearColor InLinearColor = InColor.ReinterpretAsLinear();

	// Blend the two colors using the alpha blend mode
	FLinearColor OutLinearColor = FLinearColor::LerpUsingHSV(InLinearColor, RedColor, RedColor.A);

	// Convert the output FLinearColor back to FColor
	FColor OutColor = OutLinearColor.ToFColor(true); // true means to preserve the alpha value

	// Return the result
	return OutColor;
}

FVector2D UCaptureManager::ProjectWorldPointToImage(FVector InWorldLocation,
                                                    USceneCaptureComponent2D* InCaptureComponent)
{
	// USceneCaptureComponent2D* InCaptureComponent = ColorCaptureComponents;
	FIntPoint InRenderTarget2DSize = FIntPoint(ScreenImageProperties.width, ScreenImageProperties.height);
	FVector2D OutPixel;
	ProjectWorldLocationToCapturedScreen(InCaptureComponent, InWorldLocation, InRenderTarget2DSize, OutPixel);
	return OutPixel;
}

void UCaptureManager::ProjectWorldPointToImageAndDraw(TArray<FColor>& ImageData, FVector InWorldLocation,
                                                      USceneCaptureComponent2D* InCaptureComponent, int32 radius = 5)
{
	FVector2D OutPixel;
	ProjectWorldPointToImage(InWorldLocation, InCaptureComponent);

	FVector CaptureLocation = InCaptureComponent->GetComponentLocation();
	FVector CaptureForward = InCaptureComponent->GetForwardVector();
	float NearClipPlane = InCaptureComponent->CustomNearClippingPlane;
	FVector LensCenter = CaptureLocation + CaptureForward * NearClipPlane;
	// Subtract the LensCenter from the InWorldLocation to get the InWorldLocation in the coordinate system of the LensCenter.
	FVector InWorldLocationInLensCenterSpace = InWorldLocation - LensCenter;

	// Draw OutPixel onto image as a red dot
	int32 x = OutPixel.X;
	int32 y = OutPixel.Y;
	int32 width = ScreenImageProperties.width;
	int32 height = ScreenImageProperties.height;
	for (int32 _i = -radius; _i <= radius; _i++)
	{
		for (int32 j = -radius; j <= radius; j++)
		{
			if (x + _i >= 0 && x + _i < width && y + j >= 0 && y + j < height)
			// TODO: If it's not in the bounds it's not being captured. Check this beforehand and exit.
			{
				FColor color = ImageData[(y + j) * width + (x + _i)];
				ImageData[(y + j) * width + (x + _i)] = AddTransparentRed(color);
			}
		}
	}
}

void UCaptureManager::FColorImgToB64(TArray<FColor>& ImageData, FString& base64) const
{
	TArray64<uint8> DstData;
	FImageUtils::PNGCompressImageArray(ScreenImageProperties.width, ScreenImageProperties.height, ImageData,
	                                   DstData);
	// Convert data to base64 string
	DstData.Shrink(); // Shrink the source array to remove any extra slack
	uint8* SrcPtr = DstData.GetData(); // Get a pointer to the raw data of the source array
	int32 SrcCount = DstData.Num(); // Get the number of elements in the source array
	TArray<uint8> NDstData(SrcPtr, SrcCount);
	base64 = FBase64::Encode(NDstData);
}

void UCaptureManager::SendImageToServer(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2) const
{
	// Compress image data to PNG format
	FString base64_1;
	FColorImgToB64(ImageData1, base64_1);
	FString base64_2;
	FColorImgToB64(ImageData2, base64_2);

	// Create json object and emit to socket
	auto JsonObject = USIOJConvert::MakeJsonObject();
	auto AddPostfixtoname = [](const FString& name, const FString postfix) -> FString
	{
		return name + postfix;
	};
	JsonObject->SetStringField(TEXT("name1"), AddPostfixtoname(InstanceName, FString("_1")));
	JsonObject->SetStringField(TEXT("name2"), AddPostfixtoname(InstanceName, FString("_2")));
	JsonObject->SetStringField(TEXT("image1"), base64_1);
	JsonObject->SetStringField(TEXT("image2"), base64_2);

	// send TMap<FString, TArray<float>> MapTagToPixelData to server by creating a new flaot array and adding the size of each array and then the array itself
	TArray<float> locPixelLocationAndDistanceArray;
	for (auto& Elem : MapTagToPixelData)
	{
		FString tag = Elem.Key;
		TArray<float> PixelData = Elem.Value;
		locPixelLocationAndDistanceArray.Add(MapTagToPixelData[tag].Num());
		locPixelLocationAndDistanceArray.Append(PixelData);
	}
	// convert to makeshareable jsonvaluenumber array
	TArray<TSharedPtr<FJsonValue>> arr2;
	for (auto& elem2 : locPixelLocationAndDistanceArray)
	{
		arr2.Add(MakeShareable(new FJsonValueNumber(elem2)));
	}
	JsonObject->SetArrayField(TEXT("arr2"), arr2);
	
	// do above but use fjsonvalue and tshared pointer
	// TArray<TSharedPtr<FJsonValue>> arr;
	// for (auto& elem : MapTagToPixelData)
	// {
	// 	FString tag = elem.Key;
	// 	TArray<float> PixelData = elem.Value;
	// 	TArray<TSharedPtr<FJsonValue>> arr2;
	// 	for (auto& elem2 : PixelData)
	// 	{
	// 		arr2.Add(MakeShareable(new FJsonValueNumber(elem2)));
	// 	}
	// 	arr.Add(MakeShareable(new FJsonValueObject(MakeShareable(new FJsonObject()))));
	// 	arr.Last()->AsObject()->SetArrayField(TEXT("arr"), arr2);
	// 	arr.Last()->AsObject()->SetStringField(TEXT("tag"), tag);		
	// }
	// JsonObject->SetArrayField(TEXT("arr"), arr); 
	
	SIOClientComponent->EmitNative(TEXT("imageJson"), JsonObject);
}

void UCaptureManager::DoImageSegmentation(TArray<FColor>& ImageData, USceneCaptureComponent2D* InCaptureComponent)
{
	// Get actors
	TArray<AActor*> FoundActors;
	TSet<FString> MyStrings = {"Tree", "Wall"};
	for (auto& Str : MyStrings)
	{
		TArray<AActor*> TempFoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(*Str), TempFoundActors);
		FoundActors.Append(TempFoundActors);
	}

	ParallelFor(FoundActors.Num(), [&](int32 i) -> void
	{
		const AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(FoundActors[i]);
		if (!StaticMeshActor)
		{
			return;
		}
		const UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
		if (!StaticMeshComponent)
		{
			return;
		}
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (!StaticMesh)
		{
			return;
		}

		FTransform ComponentToWorldTransform = StaticMeshComponent->GetComponentTransform();
		TArray<FVector> Vertices = GetOutlineOfStaticMesh(StaticMesh, ComponentToWorldTransform);

		for (int vi = 0; vi < Vertices.Num(); vi++)
		{
			const FVector InWorldLocation = Vertices[vi];
			ProjectWorldPointToImageAndDraw(ImageData, InWorldLocation, InCaptureComponent, 1);
		}
	});
}

void UCaptureManager::ColorImageObjects(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2)
{
	USceneCaptureComponent2D* CaptureComponent = ColorCapture->GetCaptureComponent2D();
	// clear map tag to pixel data
	MapTagToPixelData.Empty();
	for (int i = 0; i < ImageData1.Num(); i++)
	{
		// log pixel
		// UE_LOG(LogTemp, Warning, TEXT("Pixel: %s"), *ImageData1[i].ToString());
		FColor& color1 = ImageData1[i];
		FColor& color2 = ImageData2[i];
		if (const auto val = MapOfMarks.Find(color1.R) != nullptr)
		{
			auto tag = MapOfMarks[color1.R].Key;
			int32 x = i % ScreenImageProperties.width;
			int32 y = i / ScreenImageProperties.width;
			
			// MapTagToPixelLocationAndDistance.FindOrAdd(tag).Add(TPair<FVector2d, float>(FVector2d(i % ScreenImageProperties.width, i / ScreenImageProperties.width), color2.R));
			// PixelLocationAndDistanceArray.Add(i % ScreenImageProperties.width);
			// PixelLocationAndDistanceArray.Add(i / ScreenImageProperties.width);
			// PixelLocationAndDistanceArray.Add(color2.R); // placeholder still get real distance
			// MapTagToPixelLocationAndDistanceSize.FindOrAdd(tag)++;
			MapTagToPixelData.FindOrAdd(tag).Add(x);
			MapTagToPixelData.FindOrAdd(tag).Add(y);
			ImageData2[i] = MapOfMarks[color1.R].Value;
			// log color1.r
			// UE_LOG(LogTemp, Warning, TEXT("Color1.R: %d"), color1.R);

			// distance
			
			// FVector WorldPosition;
			// FVector WorldDirection;
			// UGameplayStatics::DeprojectSceneCaptureToWorld(ColorCapture.Get(), FVector2d(x, y), WorldPosition,
			//                                                WorldDirection);
			// UE_LOG(LogTemp, Warning, TEXT("WorldPosition: %s"), *WorldPosition.ToString());
			// UE_LOG(LogTemp, Warning, TEXT("WorldDirection: %s"), *WorldDirection.ToString());
			// draw debug point
			// DrawDebugPoint(GetWorld(), WorldPosition, 5, FColor::Red, false, 0.1f);
		}
	}
}

void UCaptureManager::ProcessImageData(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2)
{
	// Segmentation
	// DoImageSegmentation(ImageData, InCaptureComponent);

	ColorImageObjects(ImageData1, ImageData2);

	SendImageToServer(ImageData1, ImageData2);
}

bool UCaptureManager::ProjectWorldLocationToCapturedScreen(USceneCaptureComponent2D* InCaptureComponent,
                                                           const FVector& InWorldLocation,
                                                           const FIntPoint& InRenderTarget2DSize,
                                                           FVector2D& OutPixel)
{
	// Render Target's Rectangle
	verify(InRenderTarget2DSize.GetMin() > 0);
	const FIntRect renderTargetRect(0, 0, InRenderTarget2DSize.X, InRenderTarget2DSize.Y);

	// Initialise Viewinfo for projection matrix from [InCaptureComponent]
	FMinimalViewInfo minimalViewInfo;
	InCaptureComponent->GetCameraView(0.f, minimalViewInfo);

	// Fetch [captureComponent]'s [CustomProjectionMatrix]
	TOptional<FMatrix> customProjectionMatrix;
	if (InCaptureComponent->bUseCustomProjectionMatrix)
	{
		customProjectionMatrix = InCaptureComponent->CustomProjectionMatrix;
	}

	// Calculate [cameraViewProjectionMatrix]
	FMatrix captureViewMatrix, captureProjectionMatrix, captureViewProjectionMatrix;
	UGameplayStatics::CalculateViewProjectionMatricesFromMinimalView(minimalViewInfo, customProjectionMatrix,
	                                                                 captureViewMatrix, captureProjectionMatrix,
	                                                                 captureViewProjectionMatrix);

	bool result = FSceneView::ProjectWorldToScreen(InWorldLocation, renderTargetRect,
	                                               captureViewProjectionMatrix, OutPixel);

	// UE_LOG(LogTemp, Warning,
	//        TEXT(
	// 	       "ON [%s] CAPTURED SCREEN: WORLD LOCATION [%s] HAS LOCAL PIXEL COORDINATES: (X) %lf [over %d] OR (Y) %lf [over %d]"
	//        ),
	//        *InCaptureComponent->GetName(), *InWorldLocation.ToString(),
	//        OutPixel.X, InRenderTarget2DSize.X,
	//        OutPixel.Y, InRenderTarget2DSize.Y);
	return result;
}

/**
 * @brief Captures frame every frameMod frame
 * @param DeltaTime 
 * @param TickType 
 * @param ThisTickFunction
 */
void UCaptureManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// log the position of all the capture components
	// UE_LOG(LogTemp, Warning, TEXT("ColorCapture: %s"), *ColorCapture->GetActorLocation().ToString());
	// UE_LOG(LogTemp, Warning, TEXT("SegmentationCapture: %s"), *SegmentationCapture->GetActorLocation().ToString());
	// UE_LOG(LogTemp, Warning, TEXT("myscenecap catpure component: %s"), *MySceneCap->GetComponentLocation().ToString());

	// capture frame every frameMod frame
	if (frameCount++ % frameMod == 0)
	{
		// Capture render target data (adds render request to queue)
		CaptureColorNonBlocking(SegmentationCapture->GetCaptureComponent2D(), true);
		frameCount = 1;
	}
	if (!RenderRequestQueue.IsEmpty())
	{
		FRenderRequest* nextRenderRequest = nullptr;
		RenderRequestQueue.Peek(nextRenderRequest);
		if (nextRenderRequest)
		{
			if (nextRenderRequest->RenderFence.IsFenceComplete())
			{
				ProcessImageData(nextRenderRequest->Image1, nextRenderRequest->Image2);
				RenderRequestQueue.Pop();
				delete nextRenderRequest;
			}
		}
	}
}
