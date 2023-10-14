#include "MowerVehicleMovementComponent.h"

#include "PhysicalMaterials/PhysicalMaterial.h"


void UMowerVehicleSimulation::UpdateSimulation(float DeltaTime, const FChaosVehicleAsyncInput& InputData,
	Chaos::FRigidBodyHandle_Internal* Handle)
{
	UChaosVehicleSimulation::UpdateSimulation(DeltaTime, InputData, Handle); // skip the direct parent class, we are overriding everything

	if (CanSimulate() && Handle)
	{
		// sanity check that everything is setup ok
		ensure(PVehicle->Wheels.Num() == PVehicle->Suspension.Num());
		ensure(WheelState.LocalWheelVelocity.Num() == PVehicle->Wheels.Num());
		ensure(WheelState.WheelWorldLocation.Num() == PVehicle->Wheels.Num());
		ensure(WheelState.WorldWheelVelocity.Num() == PVehicle->Wheels.Num());

		///////////////////////////////////////////////////////////////////////
		// Cache useful state so we are not re-calculating the same data
		for (int WheelIdx = 0; WheelIdx < PVehicle->Suspension.Num(); WheelIdx++)
		{
			bool bCaptured = false;
			if (!bCaptured)
			{
				WheelState.CaptureState(WheelIdx, PVehicle->Suspension[WheelIdx].GetLocalRestingPosition(), Handle);
			}
		}

		///////////////////////////////////////////////////////////////////////
		// Engine/Transmission
		if (PVehicle->bMechanicalSimEnabled)
		{
			// ProcessMechanicalSimulation(DeltaTime);
			MyMechanicalSimulation(InputData.PhysicsInputs.NetworkInputs.VehicleInputs, DeltaTime);
		}

		///////////////////////////////////////////////////////////////////////
		// Suspension

		if (PVehicle->bSuspensionEnabled)
		{
			ApplySuspensionForces(DeltaTime, InputData.PhysicsInputs.WheelTraceParams);
		}

		///////////////////////////////////////////////////////////////////////
		// Steering

		ProcessSteering(InputData.PhysicsInputs.NetworkInputs.VehicleInputs);

		///////////////////////////////////////////////////////////////////////
		// Wheel Friction

		if (PVehicle->bWheelFrictionEnabled)
		{
			ApplyWheelFrictionForces(DeltaTime);
		}
	}
}

void UMowerVehicleSimulation::ProcessSteering(const FControlInputs& ControlInputs)
{
	using namespace Chaos;

	auto& PSteering = PVehicle->GetSteering();

	for (int WheelIdx = 0; WheelIdx < PVehicle->Wheels.Num(); WheelIdx++)
	{
		auto& PWheel = PVehicle->Wheels[WheelIdx]; // Physics Wheel
		FHitResult& HitResult = WheelState.TraceResult[WheelIdx];

		if (PWheel.SteeringEnabled)
		{
			float SpeedScale = 1.0f;

			// allow full counter steering when steering into a power slide
			//if (ControlInputs.SteeringInput * VehicleState.VehicleLocalVelocity.Y > 0.0f)
			{
				SpeedScale = PVehicle->GetSteering().GetSteeringFromVelocity(CmSToMPH(VehicleState.ForwardSpeed));
			}

			float SteeringAngle = ControlInputs.SteeringInput * SpeedScale;

			
			float WheelSide = PVehicle->GetSuspension(WheelIdx).GetLocalRestingPosition().Y;
			SteeringAngle = PSteering.GetSteeringAngle(SteeringAngle, PWheel.MaxSteeringAngle, WheelSide);

			FVector AngularVelocity = VehicleState.VehicleWorldTransform.TransformVectorNoScale(VehicleState.VehicleWorldAngularVelocity);
			AngularVelocity = AngularVelocity * 180.f / PI;
			// PWheel.SetSteeringAngle(AngularVelocity.Z);
			
		}
		else
		{
			// PWheel.SetSteeringAngle(0.0f);
		}
	}
}

void UMowerVehicleSimulation::ApplyWheelFrictionForces(float DeltaTime)
{
	using namespace Chaos;

	for (int WheelIdx = 0; WheelIdx < PVehicle->Wheels.Num(); WheelIdx++)
	{
		auto& PWheel = PVehicle->Wheels[WheelIdx]; // Physics Wheel
		FHitResult& HitResult = WheelState.TraceResult[WheelIdx];

		if (PWheel.InContact())
		{
			if (HitResult.PhysMaterial.IsValid())
			{
				PWheel.SetSurfaceFriction(HitResult.PhysMaterial->Friction);
			}

			// take into account steering angle
			float SteerAngleDegrees = PWheel.SteeringAngle;
			FRotator SteeringRotator(0.f, SteerAngleDegrees, 0.f);
			FVector SteerLocalWheelVelocity = SteeringRotator.UnrotateVector(WheelState.LocalWheelVelocity[WheelIdx]);

			PWheel.SetVehicleGroundSpeed(SteerLocalWheelVelocity);
			PWheel.Simulate(DeltaTime);

			float RotationAngle = FMath::RadiansToDegrees(PWheel.GetAngularPosition());
			FVector FrictionForceLocal = PWheel.GetForceFromFriction();
			FrictionForceLocal = SteeringRotator.RotateVector(FrictionForceLocal);

			FVector GroundZVector = HitResult.Normal;
			FVector GroundXVector = FVector::CrossProduct(VehicleState.VehicleRightAxis, GroundZVector);
			FVector GroundYVector = FVector::CrossProduct(GroundZVector, GroundXVector);
			
			FMatrix Mat = FMatrix(GroundXVector, GroundYVector, GroundZVector, VehicleState.VehicleWorldTransform.GetLocation());
			FVector FrictionForceVector = Mat.TransformVector(FrictionForceLocal);

			check(PWheel.InContact());
			if (PVehicle->bLegacyWheelFrictionPosition)
			{
				AddForceAtPosition(FrictionForceVector, WheelState.WheelWorldLocation[WheelIdx]);
			}
			else
			{
				// UE_LOG(LogTemp, Warning, TEXT("FrictionForceVector: %s"), *FrictionForceVector.ToString());
				AddForceAtPosition(FrictionForceVector, HitResult.ImpactPoint);
			}
		}
		else
		{
			PWheel.SetVehicleGroundSpeed(FVector::ZeroVector);
			PWheel.SetWheelLoadForce(0.f);
			PWheel.Simulate(DeltaTime);
		}

	}
}

void UMowerVehicleSimulation::MyMechanicalSimulation(const FControlInputs& ControlInputs, float DeltaTime)
{
	using namespace Chaos;
	float LeftThrottleInput = ControlInputs.LeftThrottleInput;
	float RightThrottleInput = ControlInputs.RightThrottleInput;
	// if one of the throttles is 0 and the other not, then we need to a apply a small force to the 0 throttle
	// to prevent the vehicle from rolling backwards. apply throttle with the same sign as the other throttle
	const float ThrottleDeadZone = 0.f;

	if (LeftThrottleInput == 0.f && RightThrottleInput != 0.f)
	{
		LeftThrottleInput = RightThrottleInput > 0.f ? ThrottleDeadZone : -ThrottleDeadZone;
	}
	else if (RightThrottleInput == 0.f && LeftThrottleInput != 0.f)
	{
		RightThrottleInput = LeftThrottleInput > 0.f ? ThrottleDeadZone : -ThrottleDeadZone;
	}
	
	for (int WheelIdx = 2; WheelIdx < PVehicle->Wheels.Num(); WheelIdx++)
	{
		float ThrottleInput = 0.f;
		if (WheelIdx == 2)
		{
			ThrottleInput = LeftThrottleInput;
		}
		else if (WheelIdx == 3)
		{
			ThrottleInput = RightThrottleInput;
		}
		auto& PWheel = PVehicle->Wheels[WheelIdx];
		// get current speed of vehicle

		// log forward speed
		// UE_LOG(LogTemp, Warning, TEXT("ForwardSpeed: %f"), VehicleState.ForwardSpeed);
		
		float Speed = CmSToKmH(VehicleState.ForwardSpeed); 
		// log speed
		// UE_LOG(LogTemp, Warning, TEXT("Speed1: %f"), Speed);
		
		// UE_LOG(LogTemp, Warning, TEXT("Speed: %f"), Speed);
		// UE_LOG(LogTemp, Warning, TEXT("ThrottleInput: %f"), ThrottleInput);
		float BaseTorque = 6000.f;
		float Torque = BaseTorque * ThrottleInput;
		// use sigmod function y=1/(1+e^(-x)) to make the torque non-linear. or use the torque curve from the engine to build curve.
		// Torque = 1.f / (1.f + FMath::Exp(-Torque));
		if(ThrottleInput > 0.f) // then they want to go forward
		{
			if(Speed < 0.f)
			{
				Torque = 6000.f;
			}
			else
			{
				// Torque = Torque * (1.f - Speed / 10.f);
				// if(Torque < 0.f)
				// {
				// 	Torque = 0.f;
				// }
				if(Speed > 10.f)
				{
					Torque = 0.f;
				}
				else
				{
					Torque = 6000.f;
				}
			}
		}
		else if (ThrottleInput < 0.f) // then they want to go backwards
		{
			if(Speed > 0.f)
			{
				Torque = -6000.f;
			}
			else
			{
				if(Speed < -10.f)
				{
					Torque = 0.f;
				}
				else
				{
					Torque = -6000.f;
				}
			}
		}
		else // then they want to stop
		{
			float AutoBrake = 1000.f;
			// if speed is positive then apply negative torque
			if(Speed > 0.f)
			{
				Torque = -AutoBrake;
			}
			else
			{
				Torque = AutoBrake;
			}
		}
		

		PWheel.SetDriveTorque(TorqueMToCm(Torque));
		
	}
}

void UMowerVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
}

void UMowerVehicleMovementComponent::UpdateState(float DeltaTime)
{
	// update input values
	AController* Controller = GetController();
	VehicleState.CaptureState(GetBodyInstance(), GetGravityZ(), DeltaTime);
	VehicleState.NumWheelsOnGround = 0;
	VehicleState.bVehicleInAir = false;
	int NumWheels = 0;
	if (PVehicleOutput)
	{
		for (int WheelIdx = 0; WheelIdx < PVehicleOutput->Wheels.Num(); WheelIdx++)
		{
			if (PVehicleOutput->Wheels[WheelIdx].InContact)
			{
				VehicleState.NumWheelsOnGround++;
			}
			else
			{
				VehicleState.bVehicleInAir = true;
			}
			NumWheels++;
		}
	}
	VehicleState.bAllWheelsOnGround = (VehicleState.NumWheelsOnGround == NumWheels);


	LeftThrottleInput = ThrottleInputRate.InterpInputValue(DeltaTime, LeftThrottleInput, LeftThrottleInput);
	RightThrottleInput = ThrottleInputRate.InterpInputValue(DeltaTime, RightThrottleInput, RightThrottleInput);
}

void UMowerVehicleMovementComponent::Update(float DeltaTime)
{
	UChaosWheeledVehicleMovementComponent::Update(DeltaTime);
	
	FChaosVehicleAsyncInput* AsyncInput = static_cast<FChaosVehicleAsyncInput*>(CurAsyncInput);

	AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.ParkingEnabled = bParkEnabled;
	AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.LeftThrottleInput = ThrottleInputRate.CalcControlFunction(LeftThrottleInput);
	AsyncInput->PhysicsInputs.NetworkInputs.VehicleInputs.RightThrottleInput = ThrottleInputRate.CalcControlFunction(RightThrottleInput);
}

TUniquePtr<Chaos::FSimpleWheeledVehicle> UMowerVehicleMovementComponent::CreatePhysicsVehicle()
{
	// Make the Vehicle Simulation class that will be updated from the physics thread async callback
	VehicleSimulationPT = MakeUnique<UMowerVehicleSimulation>();

	return UChaosVehicleMovementComponent::CreatePhysicsVehicle();
}

void UMowerVehicleMovementComponent::ProcessSleeping(const FControlInputs& ControlInputs)
{
	Super::ProcessSleeping(ControlInputs);
	VehicleState.bSleeping = false;
	VehicleState.SleepCounter = 0;
	SetSleeping(false);
}
