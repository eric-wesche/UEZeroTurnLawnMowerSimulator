// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "CaptureManager.generated.h"

// if declare this class below a place where it is used in initialization, it will cause compile error. so forward declare it here.
class AsyncInferenceTask;

USTRUCT()
struct FRenderRequest {
	GENERATED_BODY()

	TArray<FColor> Image;
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

	// Color Capture  Components
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	USceneCaptureComponent2D* ColorCaptureComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	UTextureRenderTarget2D* RenderTarget2D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SocketIO, meta = (AllowPrivateAccess = "true"))
	class USocketIOClientComponent* SIOClientComponent;

	FString InstanceName = "ddefault";
private:
	// RenderRequest Queue
	TQueue<FRenderRequest*> RenderRequestQueue;
	// inference task queue
	TQueue<FAsyncTask<AsyncInferenceTask>*> InferenceTaskQueue;
	// current inference task
	FAsyncTask<AsyncInferenceTask>* CurrentInferenceTask = nullptr;

	FScreenImageProperties ScreenImageProperties = { 0 };
	// const FModelImageProperties ModelImageProperties = { 640, 480 };
	const int32 wh = 400;
	const FModelImageProperties ModelImageProperties = { wh, wh };

	// count of total frames captured
	int frameCount = 1;
	// capture every frameMod frames
	int frameMod = 5;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	bool ProjectWorldLocationToCapturedScreen(USceneCaptureComponent2D* InCaptureComponent,
	                                          const FVector& InWorldLocation,
	                                          const FIntPoint& InRenderTarget2DSize, FVector2D& OutPixel);
	void ProjectWorldPointToImage(TArray<FColor>& ImageData, FVector InWorldLocation, int32 radius);
	TArray<FVector> GetOutlineOfStaticMesh(UStaticMesh* StaticMesh, FTransform& ComponentToWorldTransform);
	
	UFUNCTION(BlueprintCallable, Category = "ImageCapture")
	void CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation = false);

private:
	void SetupColorCaptureComponent(USceneCaptureComponent2D* CaptureComponent);
};

class AsyncInferenceTask : public FNonAbandonableTask {
public:
	AsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties ScreenImage,
		const FModelImageProperties ModelImage);
	
	// Required by UE4!
	FORCEINLINE TStatId GetStatId() const {
		RETURN_QUICK_DECLARE_CYCLE_STAT(AsyncInferenceTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	TArray<FColor> RawImageCopy;
	FScreenImageProperties ScreenImage;
	FModelImageProperties ModelImage;


public:
	void DoWork();
};
