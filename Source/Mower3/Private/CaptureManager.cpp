// Fill out your copyright notice in the Description page of Project Settings.


#include "CaptureManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHICommandList.h"
#include "SocketIOClientComponent.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"


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
    if (!ColorCaptureComponents) {
        UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents not set"));
        return;
    }
    // log defaultsubobject name
    UE_LOG(LogTemp, Warning, TEXT("ColorCaptureComponents name: %s"), *ColorCaptureComponents->GetName());
    SetupColorCaptureComponent(ColorCaptureComponents);
}

namespace {
    void ArrayFColorToUint8(const TArray<FColor>& RawImage, TArray<uint8>& InputImageCPU, int32 Width, int32 Height) {
        const int PixelCount = Width * Height;
        InputImageCPU.SetNumZeroed(PixelCount * 3);

        ParallelFor(RawImage.Num(), [&](int32 Idx) {
            const int i = Idx * 3;
            const FColor& Pixel = RawImage[Idx];

            InputImageCPU[i] = Pixel.R;
            InputImageCPU[i + 1] = Pixel.G;
            InputImageCPU[i + 2] = Pixel.B;
            });
    }
}


/**
 * @brief Initializes the render targets and material
 */
void UCaptureManager::SetupColorCaptureComponent(USceneCaptureComponent2D* CaptureComponent) {
    UObject* worldContextObject = GetWorld();

    // scene capture component render target (stores frame that is then pulled from gpu to cpu for neural network input)
    RenderTarget2D = NewObject<UTextureRenderTarget2D>();
    RenderTarget2D->InitAutoFormat(256, 256); // some random format, got crashing otherwise
    RenderTarget2D->InitCustomFormat(ModelImageProperties.width, ModelImageProperties.height, PF_B8G8R8A8, true); // PF_B8G8R8A8 disables HDR which will boost storing to disk due to less image information
    RenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    RenderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU
    RenderTarget2D->TargetGamma = 1.2f;// for Vulkan //GEngine->GetDisplayGamma(); // for DX11/12

    // add render target to scene capture component
    CaptureComponent->TextureTarget = RenderTarget2D;

    // Set Camera Properties
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    // CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
    // CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_Normal;
   
    CaptureComponent->ShowFlags.SetTemporalAA(true);
}

/**
 * @brief Sends request to gpu to read frame (send from gpu to cpu)
 * @param CaptureComponent 
 * @param IsSegmentation 
 */
void UCaptureManager::CaptureColorNonBlocking(USceneCaptureComponent2D* CaptureComponent, bool IsSegmentation) {
    if (!IsValid(CaptureComponent)) {
        UE_LOG(LogTemp, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
        return;
    }

    FTextureRenderTargetResource* renderTargetResource = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
    
    const int32& rtx = CaptureComponent->TextureTarget->SizeX;
    const int32& rty = CaptureComponent->TextureTarget->SizeY;
    
    struct FReadSurfaceContext {
        FRenderTarget* SrcRenderTarget;
        TArray<FColor>* OutData;
        FIntRect Rect;
        FReadSurfaceDataFlags Flags;
    };

    // Init new RenderRequest
    FRenderRequest* renderRequest = new FRenderRequest();
    renderRequest->isPNG = IsSegmentation;

    int32 width = rtx; 
    int32 height = rty;
    ScreenImageProperties = { width, height };

    // Setup GPU command. send the same command again but use the render target that is in the widget, and modify it to add the box
    FReadSurfaceContext readSurfaceContext = {
        renderTargetResource,
        &(renderRequest->Image), // store frame in render request
        FIntRect(0,0,width, height),
        FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
    };
    
    ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
        [readSurfaceContext](FRHICommandListImmediate& RHICmdList) {
            RHICmdList.ReadSurfaceData(
                readSurfaceContext.SrcRenderTarget->GetRenderTargetTexture(),
                readSurfaceContext.Rect,
                *readSurfaceContext.OutData,
                readSurfaceContext.Flags
            );
        });

    // Add new task to RenderQueue
    RenderRequestQueue.Enqueue(renderRequest);

    // Set RenderCommandFence
    // TODO: should pass true or false?
    renderRequest->RenderFence.BeginFence(false);
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

    if (!InferenceTaskQueue.IsEmpty()) { // Check if there is a task in queue and start it, and delete the old one
        if (CurrentInferenceTask == nullptr || CurrentInferenceTask->IsDone()) {
            FAsyncTask<AsyncInferenceTask>* task = nullptr;
            InferenceTaskQueue.Dequeue(task);
            task->StartBackgroundTask();
            if (CurrentInferenceTask != nullptr && CurrentInferenceTask->IsDone()) delete CurrentInferenceTask;
            CurrentInferenceTask = task;
        }
    }

    if (frameCount++ % frameMod == 0) { // capture every frameMod frame
        // Capture Color Image (adds render request to queue)
        CaptureColorNonBlocking(ColorCaptureComponents, true);
        frameCount = 1;
    }
    // If there is a render task in the queue, read pixels once RenderFence is completed
    if (!RenderRequestQueue.IsEmpty()) {
        // Peek the next RenderRequest from queue
        FRenderRequest* nextRenderRequest = nullptr;
        RenderRequestQueue.Peek(nextRenderRequest);
        if (nextRenderRequest) { // nullptr check
            if (nextRenderRequest->RenderFence.IsFenceComplete()) { // Check if rendering is done, indicated by RenderFence
               
                // Get image for this frame
                TArray<FColor> ImageData = nextRenderRequest->Image;

                
                TArray<AActor*> FoundActors;
                UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("Tree")), FoundActors);
                for (int32 i = 0; i < FoundActors.Num(); i++)
                {
                    FVector InWorldLocation = FoundActors[i]->GetActorLocation();
                    USceneCaptureComponent2D* InCaptureComponent = ColorCaptureComponents;
                    FIntPoint InRenderTarget2DSize = FIntPoint(ScreenImageProperties.width, ScreenImageProperties.height);
                    FVector2D OutPixel;
                    ProjectWorldLocationToCapturedScreen(InCaptureComponent, InWorldLocation, InRenderTarget2DSize, OutPixel);
                    // Draw OutPixel onto image as a red dot
                    int32 x = OutPixel.X;
                    int32 y = OutPixel.Y;
                    int32 width = ScreenImageProperties.width;
                    int32 height = ScreenImageProperties.height;
                    int32 radius = 5;
                    for (int32 _i = -radius; _i <= radius; _i++) {
                        for (int32 j = -radius; j <= radius; j++) {
                            if (x + _i >= 0 && x + _i < width && y + j >= 0 && y + j < height) {
                                ImageData[(y + j) * width + (x + _i)] = FColor(255, 0, 0, 255);
                            }
                        }
                    }
                }

                // Do above but for every point in the volumne of the first actor found with tag tree, so as to fill the tree with red
                // for (int32 i = 0; i < FoundActors.Num(); i++) {
                //     InWorldLocation = FoundActors[i]->GetActorLocation();
                //     ProjectWorldLocationToCapturedScreen(InCaptureComponent, InWorldLocation, InRenderTarget2DSize, OutPixel);
                //     int32 x = OutPixel.X;
                //     int32 y = OutPixel.Y;
                //     int32 width = ScreenImageProperties.width;
                //     int32 height = ScreenImageProperties.height;
                //     int32 radius = 5;
                //     for (i = -radius; i <= radius; i++) {
                //         for (int32 j = -radius; j <= radius; j++) {
                //             if (x + i >= 0 && x + i < width && y + j >= 0 && y + j < height) {
                //                 ImageData[(y + j) * width + (x + i)] = FColor(255, 0, 0, 255);
                //             }
                //         }
                //     }
                // }
                
                 
                
                // Compress image data to PNG format
                TArray64<uint8> DstData;
                FImageUtils::PNGCompressImageArray(ScreenImageProperties.width, ScreenImageProperties.height, ImageData,
                    DstData);

                // Convert data to base64 string
                DstData.Shrink(); // Shrink the source array to remove any extra slack
                uint8* SrcPtr = DstData.GetData(); // Get a pointer to the raw data of the source array
                int32 SrcCount = DstData.Num(); // Get the number of elements in the source array
                TArray<uint8> NDstData(SrcPtr, SrcCount); // Create the destination array using the pointer and the count
                FString base64 = FBase64::Encode(NDstData);

                // Create json object and emit to socket
                auto JsonObject = USIOJConvert::MakeJsonObject();
                JsonObject->SetStringField(TEXT("name"), InstanceName);
                JsonObject->SetStringField(TEXT("image"), base64);
                SIOClientComponent->EmitNative(TEXT("imageJson"), JsonObject);
                
                // log emitting image for capture manager name
                // UE_LOG(LogTemp, Warning, TEXT("Emitting image for capture manager name: %s"), *InstanceName);
                auto val = InstanceName;
                // create new inference task
                FAsyncTask<AsyncInferenceTask>* MyTask =
                    new FAsyncTask<AsyncInferenceTask>(nextRenderRequest->Image, ScreenImageProperties, ModelImageProperties);
                InferenceTaskQueue.Enqueue(MyTask);
                // Delete the first element from RenderQueue
                RenderRequestQueue.Pop();
                delete nextRenderRequest;
            }
        }
    }
}

bool UCaptureManager::ProjectWorldLocationToCapturedScreen(USceneCaptureComponent2D* InCaptureComponent, const FVector& InWorldLocation,
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
    if(InCaptureComponent->bUseCustomProjectionMatrix) {
        customProjectionMatrix = InCaptureComponent->CustomProjectionMatrix;
    }

    // Calculate [cameraViewProjectionMatrix]
    FMatrix captureViewMatrix, captureProjectionMatrix, captureViewProjectionMatrix;
    UGameplayStatics::CalculateViewProjectionMatricesFromMinimalView(minimalViewInfo, customProjectionMatrix,
        captureViewMatrix, captureProjectionMatrix, captureViewProjectionMatrix);

    bool result = FSceneView::ProjectWorldToScreen(InWorldLocation, renderTargetRect,
                                                   captureViewProjectionMatrix, OutPixel);

    UE_LOG(LogTemp, Verbose, TEXT("ON [%s] CAPTURED SCREEN: WORLD LOCATION [%s] HAS LOCAL PIXEL COORDINATES: (X) %lf [over %d] OR (Y) %lf [over %d]"),
                             *InCaptureComponent->GetName(), *InWorldLocation.ToString(),
                             OutPixel.X, InRenderTarget2DSize.X,
                             OutPixel.Y, InRenderTarget2DSize.Y);
    return result;
}

AsyncInferenceTask::AsyncInferenceTask(const TArray<FColor>& RawImage, const FScreenImageProperties ScreenImage,
    const FModelImageProperties ModelImage) {
    this->RawImageCopy = RawImage;
    this->ScreenImage = ScreenImage;
    this->ModelImage = ModelImage;
}

void AsyncInferenceTask::DoWork() {
    //log do work
    // UE_LOG(LogTemp, Warning, TEXT("AsyncTaskDoWork inference"));
    
}

