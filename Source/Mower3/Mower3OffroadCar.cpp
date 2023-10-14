// Copyright Epic Games, Inc. All Rights Reserved.


#include "Mower3OffroadCar.h"

#include "CaptureManager.h"
#include "Mower3OffroadWheelFront.h"
#include "Mower3OffroadWheelRear.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "MowerVehicleMovementComponent.h"
#include "SocketIOClientComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/SpringArmComponent.h"

AMower3OffroadCar::AMower3OffroadCar(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer.SetDefaultSubobjectClass<UMowerVehicleMovementComponent>(VehicleMovementComponentName))
{
	// construct the mesh components
	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	Chassis->SetupAttachment(GetMesh());

	// NOTE: tire sockets are set from the Blueprint class
	TireFrontLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Front Left"));
	TireFrontLeft->SetupAttachment(GetMesh(), FName("VisWheel_FL"));
	TireFrontLeft->SetCollisionProfileName(FName("NoCollision"));

	TireFrontRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Front Right"));
	TireFrontRight->SetupAttachment(GetMesh(), FName("VisWheel_FR"));
	TireFrontRight->SetCollisionProfileName(FName("NoCollision"));
	TireFrontRight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	TireRearLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Rear Left"));
	TireRearLeft->SetupAttachment(GetMesh(), FName("VisWheel_BL"));
	TireRearLeft->SetCollisionProfileName(FName("NoCollision"));

	TireRearRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Rear Right"));
	TireRearRight->SetupAttachment(GetMesh(), FName("VisWheel_BR"));
	TireRearRight->SetCollisionProfileName(FName("NoCollision"));
	TireRearRight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	// adjust the cameras
	GetFrontSpringArm()->SetRelativeLocation(FVector(-5.0f, -30.0f, 135.0f));
	GetBackSpringArm()->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));

	// Note: for faster iteration times, the vehicle setup can be tweaked in the Blueprint instead

	// Set up the chassis
	GetChaosVehicleMovement()->ChassisHeight = 160.0f;
	GetChaosVehicleMovement()->DragCoefficient = 0.1f;
	GetChaosVehicleMovement()->DownforceCoefficient = 0.1f;
	GetChaosVehicleMovement()->CenterOfMassOverride = FVector(0.0f, 0.0f, 75.0f);
	GetChaosVehicleMovement()->bEnableCenterOfMassOverride = true;

	// Set up the wheels
	GetChaosVehicleMovement()->bLegacyWheelFrictionPosition = true;
	GetChaosVehicleMovement()->WheelSetups.SetNum(4);

	GetChaosVehicleMovement()->WheelSetups[0].WheelClass = UMower3OffroadWheelFront::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[0].BoneName = FName("PhysWheel_FL");
	GetChaosVehicleMovement()->WheelSetups[0].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->WheelSetups[1].WheelClass = UMower3OffroadWheelFront::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[1].BoneName = FName("PhysWheel_FR");
	GetChaosVehicleMovement()->WheelSetups[1].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->WheelSetups[2].WheelClass = UMower3OffroadWheelRear::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[2].BoneName = FName("PhysWheel_BL");
	GetChaosVehicleMovement()->WheelSetups[2].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->WheelSetups[3].WheelClass = UMower3OffroadWheelRear::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[3].BoneName = FName("PhysWheel_BR");
	GetChaosVehicleMovement()->WheelSetups[3].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	// Set up the engine
	// NOTE: Check the Blueprint asset for the Torque Curve
	GetChaosVehicleMovement()->EngineSetup.MaxTorque = 600.0f;
	GetChaosVehicleMovement()->EngineSetup.MaxRPM = 5000.0f;
	GetChaosVehicleMovement()->EngineSetup.EngineIdleRPM = 1200.0f;
	GetChaosVehicleMovement()->EngineSetup.EngineBrakeEffect = 0.05f;
	GetChaosVehicleMovement()->EngineSetup.EngineRevUpMOI = 5.0f;
	GetChaosVehicleMovement()->EngineSetup.EngineRevDownRate = 600.0f;

	// Set up the differential
	GetChaosVehicleMovement()->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;
	GetChaosVehicleMovement()->DifferentialSetup.FrontRearSplit = 0.5f;

	// Set up the steering
	// NOTE: Check the Blueprint asset for the Steering Curve
	GetChaosVehicleMovement()->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	GetChaosVehicleMovement()->SteeringSetup.AngleRatio = 0.7f;

	MySceneCapture1 = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MySceneCapture1"));
	MySceneCapture1->SetupAttachment(GetMesh());

	MyCaptureManager = CreateDefaultSubobject<UCaptureManager>(TEXT("MyCaptureManager"));
	
	check(MyCaptureManager);
	MyCaptureManager->ColorCaptureComponents = MySceneCapture1;
	
	SIOClientComponent = CreateDefaultSubobject<USocketIOClientComponent>(TEXT("SocketIOClientComponent"));
	SIOClientComponent->URLParams.AddressAndPort = TEXT("http://127.0.0.1:8000");
	SIOClientComponent->URLParams.Path = TEXT("");

	MyCaptureManager->SIOClientComponent = SIOClientComponent;
}

void AMower3OffroadCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ChaosVehicleMovement->SetLeftThrottleInput(-1.f);
	ChaosVehicleMovement->SetRightThrottleInput(1.f);

	ReplaceOrRemoveGrass(false);
	// ReplaceOrRemoveGrass(true);
}

void AMower3OffroadCar::BeginPlay()
{
	Super::BeginPlay();
	
	SIOClientComponent->OnNativeEvent(TEXT("my response"), [this](const FString& Event,
																  const TSharedPtr<FJsonValue>& Message)
	{
		UE_LOG(LogTemp, Warning, TEXT("Received: %s"), *USIOJConvert::ToJsonString(Message));
	});
}

void AMower3OffroadCar::ReplaceOrRemoveGrass(const bool bDebug, const FString& grassNameToReplace)
{
	const bool isGrassNameEmpty = grassNameToReplace.IsEmpty();
	const FVector fl = TireFrontLeft->GetComponentLocation();
	const FVector fr = TireFrontRight->GetComponentLocation();
	const double fd2d = FVector2D::Distance(FVector2D(fl.X, fl.Y), FVector2D(fr.X, fr.Y)); // front width
	
	const FVector bl = TireRearLeft->GetComponentLocation();
	const double bd2d = FVector2D::Distance(FVector2D(fl.X, fl.Y), FVector2D(bl.X, bl.Y)); // left length
	
	const UE::Math::TVector2<double> fl2d = FVector2D(fl.X, fl.Y);
	const UE::Math::TVector2<double> fr2d = FVector2D(fr.X, fr.Y);
	const UE::Math::TVector2<double> width2d = fr2d - fl2d;
	const UE::Math::TVector2<double> bl2d = FVector2D(bl.X, bl.Y);
	const UE::Math::TVector2<double> length2d = bl2d - fl2d;
	
	const UE::Math::TVector<double> CenterLength = fl + FVector(length2d.X / 2, length2d.Y / 2, 0);
	const double Height = TireFrontLeft->GetStaticMesh()->GetBounds().BoxExtent.Z;
	UE::Math::TVector<double> Center = fl + FVector(width2d.X / 2 + length2d.X / 2, width2d.Y / 2 + length2d.Y / 2, 0);

	// The width is currently outside of the wheels, so we need to add the wheel radius to the width
	const double WheelX = TireFrontLeft->GetStaticMesh()->GetBounds().BoxExtent.X;
	const double OffsetX = WheelX/2;
	const double WheelY = TireFrontLeft->GetStaticMesh()->GetBounds().BoxExtent.Y;
	const double OffsetY = WheelY/2;
	const FVector HalfSize = FVector(fd2d/2 - OffsetY, bd2d/2 - OffsetX, Height/2);

	const UWorld* World = GetWorld();
	const double LengthAngle = (CenterLength-fl).HeadingAngle();
	const FQuat ShapeRotation = FQuat(FVector(0, 0, 1), LengthAngle);
	if(bDebug)
	{
		DrawDebugLine(GetWorld(), fl, fl + FVector(width2d.X, width2d.Y, 0), FColor::Red, false, 0);
		DrawDebugLine(GetWorld(), fl, fl + FVector(length2d.X, length2d.Y, 0), FColor::Red, false, 0);
		DrawDebugBox(World, Center, HalfSize, ShapeRotation, FColor::Green, false, 0);
	}

	const ECollisionChannel TraceChannel = ECC_WorldDynamic;
	TArray<FHitResult> HitResults;

	if (World)
	{
		bool bHit = World->SweepMultiByChannel(HitResults, Center, Center, ShapeRotation, TraceChannel, FCollisionShape::MakeBox(HalfSize));

		if (bHit)
		{
			UFoliageInstancedStaticMeshComponent* NewFoliageComp = NewObject<UFoliageInstancedStaticMeshComponent>(this);
			NewFoliageComp->SetStaticMesh(ParentStaticMesh);
			NewFoliageComp->RegisterComponent();
			TArray<FTransform> NewInstanceTransforms;
		
			for(FHitResult HitResult: HitResults)
			{ // TODO: parallel for
				UFoliageInstancedStaticMeshComponent* FoliageComp = Cast<UFoliageInstancedStaticMeshComponent>(HitResult.GetComponent());
				if(!FoliageComp)
				{
					continue;
				}

				int32 InstanceIndex = HitResult.Item;

				if(isGrassNameEmpty)
				{
					FoliageComp->RemoveInstance(InstanceIndex);
					continue;
				}
				else
				{
					const FString name1 = FoliageComp->GetStaticMesh()->GetName();
					if(!name1.Contains(grassNameToReplace))
					{
						continue;
					}
			
					FTransform InstanceTransform;
					FoliageComp->GetInstanceTransform(InstanceIndex, InstanceTransform, true);
					FoliageComp->RemoveInstance(InstanceIndex);
					const FString name = FoliageComp->GetStaticMesh()->GetName();
					NewInstanceTransforms.Add(InstanceTransform);
				}
			}
			if(!isGrassNameEmpty)
			{
				NewFoliageComp->AddInstances(NewInstanceTransforms, false);
			}
		}
	}
}