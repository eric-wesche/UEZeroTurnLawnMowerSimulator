// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mower3Pawn.h"
#include "Mower3OffroadCar.generated.h"



USTRUCT()
struct FAiVehicleInputs
{
	GENERATED_USTRUCT_BODY()

	FAiVehicleInputs() :
		  LeftThrottleInput(0.f)
		, RightThrottleInput(0.f)
	{}

	UPROPERTY()
	float LeftThrottleInput;
	UPROPERTY()
	float RightThrottleInput;
};

class UCaptureManager;
/**
 *  Offroad car wheeled vehicle implementation
 */
UCLASS(abstract)
class MOWER3_API AMower3OffroadCar : public AMower3Pawn
{
	GENERATED_BODY()
	
	/** Chassis static mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Chassis;

	/** FL Tire static mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireFrontLeft;

	/** FR Tire static mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireFrontRight;

	/** RL Tire static mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireRearLeft;

	/** RR Tire static mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireRearRight;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Capture, meta = (AllowPrivateAccess = "true"))
	USceneCaptureComponent2D* MySceneCapture1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Capture, meta = (AllowPrivateAccess = "true"))
	USceneCaptureComponent2D* MySceneCapture4;

	UPROPERTY()
	TArray<UCaptureManager*> CaptureManagers;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SocketIO, meta = (AllowPrivateAccess = "true"))
	class USocketIOClientComponent* SIOClientComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCaptureManager* MyCaptureManager;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCaptureManager* MyCaptureManager2;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMesh* ParentStaticMesh;

private:
	FAiVehicleInputs AiVehicleInputs;
	
public:

	AMower3OffroadCar(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	// on collision
	virtual void NotifyHit( class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved,
		FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit ) override;

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	void ReplaceOrRemoveGrass( const bool bDebug = false, const FString& grassNameToReplace = "");
};
