// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CaptureManager.generated.h"

class ASceneCapture2D;

USTRUCT()
struct FRenderRequest {
	GENERATED_BODY()

	TArray<FColor> Image1;
	TArray<FColor> Image2;
	FRenderCommandFence RenderFence;
	bool isPNG;

	FRenderRequest() {
		isPNG = false;
	}
};

USTRUCT()
struct FScreenImageProperties {
	GENERATED_BODY()

	int32 width;
	int32 height;
};

USTRUCT()
struct FModelImageProperties {
	GENERATED_BODY()

	int32 width;
	int32 height;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UCaptureManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCaptureManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	ASceneCapture2D* SegmentationCapture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	TSoftObjectPtr<ASceneCapture2D> ColorCapture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	USceneCaptureComponent2D* MySceneCap;
	
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	// UTextureRenderTarget2D* RenderTarget2D;

	UPROPERTY(EditAnywhere, Category="Segmentation Setup")
	UMaterial* PostProcessMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SocketIO, meta = (AllowPrivateAccess = "true"))
	class USocketIOClientComponent* SIOClientComponent;

	FString InstanceName = "default";
private:
	// RenderRequest Queue
	TQueue<FRenderRequest*> RenderRequestQueue;

	FScreenImageProperties ScreenImageProperties = { 0 };
	// const FModelImageProperties ModelImageProperties = { 640, 480 };
	const int32 wh = 400;
	const FModelImageProperties ModelImageProperties = { wh, wh };

	// count of total frames captured
	int frameCount = 1;
	// capture every frameMod frames
	int frameMod = 5;

	const TMap<int, TPair<FString, FColor>> MapOfMarks = {
		{133, {"Wall", FColor(255, 0, 0, 255)}},
		{250, {"Tree", FColor(0, 0, 255, 255)}}
	};

	const TSet<int> ObjectTagIds = {
		133,
		250
	};

	TMap<FString, TArray<TPair<FVector2d, float>>> MapTagToPixelLocationAndDistance;

	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	void ProcessImageData(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	bool ProjectWorldLocationToCapturedScreen(USceneCaptureComponent2D* InCaptureComponent,
	                                          const FVector& InWorldLocation,
	                                          const FIntPoint& InRenderTarget2DSize, FVector2D& OutPixel);
	void ProjectWorldPointToImageAndDraw(TArray<FColor>& ImageData, FVector InWorldLocation, USceneCaptureComponent2D* InCaptureComponent, int32 radius);
	FVector2D ProjectWorldPointToImage(FVector InWorldLocation, USceneCaptureComponent2D* InCaptureComponent);
	TArray<FVector> GetOutlineOfStaticMesh(UStaticMesh* StaticMesh, FTransform& ComponentToWorldTransform);
	
	UFUNCTION(BlueprintCallable, Category = "ImageCapture")
	void CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation = false);

	void SendImageToServer(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2) const;
	void FColorImgToB64(TArray<FColor>& ImageData, FString& base64) const;
	void ColorImageObjects(TArray<FColor>& ImageData1, TArray<FColor>& ImageData2);

	void DoImageSegmentation(TArray<FColor>& ImageData, USceneCaptureComponent2D* InCaptureComponent);

private:
	void SetupColorCaptureComponent(ASceneCapture2D* ParamCapture);
	void SetupSegmentationCaptureComponent(ASceneCapture2D* ParamCapture);
	void SpawnSegmentationCaptureComponent(ASceneCapture2D* ParamCapture);
};
