// Fill out your copyright notice in the Description page of Project Settings.


#include "CaptureManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHICommandList.h"
#include "SocketIOClientComponent.h"
#include "ImageUtils.h"
#include "KismetProceduralMeshLibrary.h"
#include "MaterialDomain.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Async/ParallelFor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialExpressionSceneTexture.h"

// Sets default values for this component's properties
UCaptureManager::UCaptureManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UCaptureManager::BeginPlay()
{
	Super::BeginPlay();

	//return if ColorCaptureComponents is not set
	if (!ColorCaptureComponents)
	{
		UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents not set"));
		return;
	}
	// log defaultsubobject name
	// UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents name: %s"), *ColorCaptureComponents->GetName());
	SetupColorCaptureComponent(ColorCaptureComponents);
	SetupSegmentationCaptureComponent(ColorCaptureComponents);
}

/**
 * @brief Initializes the render targets and material
 */
void UCaptureManager::SetupColorCaptureComponent(USceneCaptureComponent2D* CaptureComponent)
{
	// UObject* worldContextObject = GetWorld();
	// scene capture component render target (stores frame that is then pulled from gpu to cpu for neural network input)
	UTextureRenderTarget2D* RenderTarget2D = NewObject<UTextureRenderTarget2D>();
	RenderTarget2D->InitAutoFormat(256, 256); // some random format, got crashing otherwise
	RenderTarget2D->InitCustomFormat(ModelImageProperties.width, ModelImageProperties.height, PF_B8G8R8A8, true);
	// PF_B8G8R8A8 disables HDR which will boost storing to disk due to less image information
	RenderTarget2D->RenderTargetFormat = RTF_RGBA8;
	// RenderTarget2D->RenderTargetFormat = RTF_RGBA8;
	RenderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU
	RenderTarget2D->TargetGamma = 1.2f; // for Vulkan //GEngine->GetDisplayGamma(); // for DX11/12

	CaptureComponent->TextureTarget = RenderTarget2D;
	// CaptureComponent->CaptureSource = SCS_SceneColorSceneDepth;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	// CaptureComponent->CompositeMode = SCCM_CustomDepth;
	// CaptureComponent->ShowFlags.SetTemporalAA(true);
}

void UCaptureManager::SetupSegmentationCaptureComponent(USceneCaptureComponent2D* ColorCapture)
{
	// Spawn SegmentaitonCaptureComponents
	// SpawnSegmentationCaptureComponent(ColorCapture);

	// Setup SegmentationCaptureComponent
	// SetupColorCaptureComponent(SegmentationCapture);
	// SegmentationCapture->FOVAngle = ColorCapture->FOVAngle;
	// auto seg1 = DuplicateObject(ColorCapture, this);
	// SetupColorCaptureComponent(seg1);
	// SegmentationCapture = seg1;

	SetupColorCaptureComponent(SegmentationCapture);

	// log fov angle
	UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents FOVAngle: %f"), ColorCaptureComponents->FOVAngle);
	UE_LOG(LogTemp, Warning, TEXT("SegmentationCapture FOVAngle: %f"), SegmentationCapture->FOVAngle);

	// log position of capture components
	// UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents position: %s"),
	//        *ColorCaptureComponents->GetComponentLocation().ToString());
	// UE_LOG(LogTemp, Warning, TEXT("SegmentationCapture position: %s"),
	// 	*SegmentationCapture->GetComponentLocation().ToString());

	// SegmentationCapture->SetWorldTransform(ColorCapture->GetComponentTransform());
	UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents position: %s"),
	       *ColorCaptureComponents->GetComponentLocation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("SegmentationCapture position: %s"),
	       *SegmentationCapture->GetComponentLocation().ToString());

	// Assign PostProcess Material
	if (PostProcessMaterial)
	{
		// PostProcessMaterial->MaterialDomain = EMaterialDomain::MD_PostProcess;
		// PostProcessMaterial->BlendableLocation = BL_BeforeTonemapping;
		//
		// // Create a scene texture sample for the post process input 0
		// UMaterialExpressionSceneTexture* PostProcessInput0 = NewObject<UMaterialExpressionSceneTexture>(PostProcessMaterial);
		// PostProcessInput0->SceneTextureId = PPI_PostProcessInput0;
		// // PostProcessMaterial->Expressions.Add(PostProcessInput0);
		// PostProcessMaterial->MaterialEx(PostProcessInput0);
		SegmentationCapture->AddOrUpdateBlendable(PostProcessMaterial);
		// SegmentationCapture->AddOrUpdateBlendable(PostProcessMaterial, .5);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PostProcessMaterial was nullptr!"));
	}
}

void UCaptureManager::SpawnSegmentationCaptureComponent(USceneCaptureComponent2D* ColorCapture)
{
	// SegmentationCapture->FOVAngle = ColorCapture->FOVAngle;
}

/**
 * @brief Sends request to gpu to read frame (send from gpu to cpu)
 * @param CaptureComponent 
 * @param IsSegmentation 
 */
void UCaptureManager::CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation)
{
	if (!IsValid(SegmentationCapture) || !IsValid(ColorCaptureComponents))
	{
		UE_LOG(LogTemp, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
		return;
	}

	FTextureRenderTargetResource* renderTargetResource1 = SegmentationCapture->TextureTarget->
	                                                                           GameThread_GetRenderTargetResource();

	const int32& rtx1 = SegmentationCapture->TextureTarget->SizeX;
	const int32& rty1 = SegmentationCapture->TextureTarget->SizeY;

	FTextureRenderTargetResource* renderTargetResource2 = ColorCaptureComponents->TextureTarget->
		GameThread_GetRenderTargetResource();

	const int32& rtx2 = ColorCaptureComponents->TextureTarget->SizeX;
	const int32& rty2 = ColorCaptureComponents->TextureTarget->SizeY;

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

	FVector CaptureLocation = ColorCaptureComponents->GetComponentLocation();
	FVector CaptureForward = ColorCaptureComponents->GetForwardVector();
	float NearClipPlane = ColorCaptureComponents->CustomNearClippingPlane;
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
	// map stencil value to color
	// 133 is the wall
	// 250 is the tree
	TMap<int, FColor> colorMap;
	colorMap.Add(133, FColor(255, 0, 0, 255));
	colorMap.Add(250, FColor(0, 255, 0, 255));
	for (int i = 0; i < ImageData1.Num(); i++)
	{
		FColor& color1 = ImageData1[i];
		if(colorMap.Contains(color1.R))
		{
			ImageData2[i] = colorMap[color1.R];
		}
	}
}

void UCaptureManager::ProcessImageData(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2)
{
	// Segmentation
	// DoImageSegmentation(ImageData, InCaptureComponent);

	// ColorImageObjects(ImageData1, ImageData2);
	
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
 * @brief If scene component is not running every frame, and this function is, then it will be reading the same data
 * from the texture over and over. TODO: check if this is true, and ensure no issues.
 * @param DeltaTime 
 * @param TickType 
 * @param ThisTickFunction
 */
void UCaptureManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!InferenceTaskQueue.IsEmpty())
	{
		// Check if there is a task in queue and start it, and delete the old one
		if (CurrentInferenceTask == nullptr || CurrentInferenceTask->IsDone())
		{
			FAsyncTask<AsyncInferenceTask>* task = nullptr;
			InferenceTaskQueue.Dequeue(task);
			task->StartBackgroundTask();
			if (CurrentInferenceTask != nullptr && CurrentInferenceTask->IsDone())
			{
				delete CurrentInferenceTask;
			}
			CurrentInferenceTask = task;
		}
	}

	USceneCaptureComponent2D* CapComp;
	bool isSegmentation = true;
	if (isSegmentation)
	{
		CapComp = SegmentationCapture;
	}
	else
	{
		CapComp = ColorCaptureComponents;
	}

	// capture every frameMod frame
	if (frameCount++ % frameMod == 0)
	{
		// Capture render target data (adds render request to queue)
		CaptureColorNonBlocking(CapComp, true);
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


// --------------------------------- ASyncInferenceTask ---------------------------------

AsyncInferenceTask::AsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties ScreenImage,
                                       const FModelImageProperties ModelImage)
{
	this->RawImageCopy = RawImage;
	this->ScreenImage = ScreenImage;
	this->ModelImage = ModelImage;
}

void AsyncInferenceTask::DoWork()
{
	// UE_LOG(LogTemp, Warning, TEXT("AsyncTaskDoWork inference"));
}
