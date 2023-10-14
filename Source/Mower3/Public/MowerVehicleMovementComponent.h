// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleManager.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "UObject/ObjectMacros.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"
#include "MowerVehicleMovementComponent.generated.h"


class UMowerVehicleSimulation : public UChaosWheeledVehicleSimulation
{
public:
	
	UMowerVehicleSimulation()
	{
		UChaosWheeledVehicleSimulation();
	}

	void UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData, Chaos::FRigidBodyHandle_Internal* Handle) override;

	void MyMechanicalSimulation(const FControlInputs& ControlInputs, float DeltaTime);

	void ProcessSteering(const FControlInputs& ControlInputs) override;

	void ApplyWheelFrictionForces(float DeltaTime) override;
};


UCLASS()
class UMowerVehicleMovementComponent : public UChaosWheeledVehicleMovementComponent
{
	GENERATED_BODY()

public:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void UpdateState(float DeltaTime) override;
	void Update(float DeltaTime) override;
	TUniquePtr<Chaos::FSimpleWheeledVehicle> CreatePhysicsVehicle() override;
	void SetLeftThrottleInput(float Value) { LeftThrottleInput = Value; SetSleeping(false); }
	void SetRightThrottleInput(float Value) { RightThrottleInput = Value; SetSleeping(false); }
	void ProcessSleeping(const FControlInputs& ControlInputs) override;

protected:
	UPROPERTY(Transient)
	float LeftThrottleInput;
	UPROPERTY(Transient)
	float RawLeftThrottleInput;
	UPROPERTY(Transient)
	float RightThrottleInput;
	UPROPERTY(Transient)
	float RawRightThrottleInput;
};

