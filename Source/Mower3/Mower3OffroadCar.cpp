// Copyright Epic Games, Inc. All Rights Reserved.


#include "Mower3OffroadCar.h"

#include "CaptureManager.h"
#include "Mower3OffroadWheelFront.h"
#include "Mower3OffroadWheelRear.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "MowerVehicleMovementComponent.h"
#include "SocketIOClientComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"

AMower3OffroadCar::AMower3OffroadCar(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer.SetDefaultSubobjectClass<UMowerVehicleMovementComponent>(VehicleMovementComponentName))
{
	MyBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Box Component"));
	MyBoxComponent->SetupAttachment(GetMesh());
	
	// construct the mesh components
	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	Chassis->SetupAttachment(GetMesh());
	// Chassis->SetNotifyRigidBodyCollision(true); // Same as setting 'Simulation Generates Hit Events' in bp. This is for physics simulation, maybe not relevant here.
	// Chassis->SetCollisionProfileName(FName("BlockAllDynamic"));
	// Chassis->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	// Chassis->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block);
	
	// NOTE: tire sockets are set from the Blueprint class
	TireFrontLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Front Left"));
	TireFrontLeft->SetupAttachment(GetMesh(), FName("VisWheel_FL"));
	// TireFrontLeft->SetCollisionProfileName(FName("NoCollision"));

	TireFrontRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Front Right"));
	TireFrontRight->SetupAttachment(GetMesh(), FName("VisWheel_FR"));
	// TireFrontRight->SetCollisionProfileName(FName("NoCollision"));
	TireFrontRight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	TireRearLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Rear Left"));
	TireRearLeft->SetupAttachment(GetMesh(), FName("VisWheel_BL"));
	// TireRearLeft->SetCollisionProfileName(FName("NoCollision"));

	TireRearRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Rear Right"));
	TireRearRight->SetupAttachment(GetMesh(), FName("VisWheel_BR"));
	// TireRearRight->SetCollisionProfileName(FName("NoCollision"));
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
	
	MyCaptureManager->ColorCaptureComponents = MySceneCapture1;

	MySceneCapture4 = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MySceneCapture4"));
	MySceneCapture4->SetupAttachment(GetMesh());

	// MyCaptureManager2 = CreateDefaultSubobject<UCaptureManager>(TEXT("MyCaptureManager2"));
	
	// MyCaptureManager2->ColorCaptureComponents = MySceneCapture4;
	
	SIOClientComponent = CreateDefaultSubobject<USocketIOClientComponent>(TEXT("SocketIOClientComponent"));
	SIOClientComponent->URLParams.AddressAndPort = TEXT("http://127.0.0.1:8000");
	SIOClientComponent->URLParams.Path = TEXT("");

	MyCaptureManager->SIOClientComponent = SIOClientComponent;
	// MyCaptureManager2->SIOClientComponent = SIOClientComponent;
}

void AMower3OffroadCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ChaosVehicleMovement->SetLeftThrottleInput(-1.f);
	// ChaosVehicleMovement->SetRightThrottleInput(1.f);

	// {
	// 	FVector CaptureLocation = MySceneCapture1->GetComponentLocation();
	// 	FRotator CaptureRotation = MySceneCapture1->GetComponentRotation();
	// 	float CaptureFOV = MySceneCapture1->FOVAngle;
	// 	DrawDebugCamera(GetWorld(), CaptureLocation, CaptureRotation, CaptureFOV, 1.0f, FColor::Blue, false, 0.0f);
	// }

	ReplaceOrRemoveGrass(false);
	// ReplaceOrRemoveGrass(true);
}

void AMower3OffroadCar::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// if other actor is a static mesh actor then log it
	if(OtherActor->IsA(AStaticMeshActor::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("OtherActor is a static mesh actor"));
	}
	
	// log tags array of other actor
	for(auto tag: OtherActor->Tags)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tag: %s"), *tag.ToString());
	}
	const TSet<FName> SetOfTagsToAccept = {"Tree", "Wall"};
	// get the tag of the other actor and check if it is in the set of tags to accept
	// first check if the other actor has any tags
	if(OtherActor->Tags.Num() == 0)
	{
		return;
	}
	const FName OtherActorTag = OtherActor->Tags[0];
	if(!SetOfTagsToAccept.Contains(OtherActorTag))
	{
		return;
	}
		
	UE_LOG(LogTemp, Warning, TEXT("OnBeginOverlap"));
	// log the overlapped component and other component
	UE_LOG(LogTemp, Warning, TEXT("OverlappedComp: %s"), *OverlappedComp->GetName());
	UE_LOG(LogTemp, Warning, TEXT("OtherComp: %s"), *OtherComp->GetName());
	// log the other actor
	UE_LOG(LogTemp, Warning, TEXT("OtherActor: %s"), *OtherActor->GetName());
	// log the other body index
	UE_LOG(LogTemp, Warning, TEXT("OtherBodyIndex: %d"), OtherBodyIndex);
	// log whether the overlap was from a sweep
	UE_LOG(LogTemp, Warning, TEXT("bFromSweep: %d"), bFromSweep);
	// log the sweep result
	UE_LOG(LogTemp, Warning, TEXT("SweepResult: %s"), *SweepResult.ToString());
	
}

void AMower3OffroadCar::BeginPlay()
{
	Super::BeginPlay();
	MyBoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AMower3OffroadCar::OnBeginOverlap);\
	
	SIOClientComponent->OnNativeEvent(TEXT("processedImage"), [this](const FString& Event,
																  const TSharedPtr<FJsonValue>& Message)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Received: %s"), *USIOJConvert::ToJsonString(Message));
		TSharedPtr<FJsonObject> JsonObject = Message->AsObject();
		// FString name = JsonObject->GetStringField("name");
		// UE_LOG(LogTemp, Warning, TEXT("Received name: %s"), *name);

		auto LeftThrottle = JsonObject->GetField<EJson::Number>("leftThrottle");
		auto RightThrottle = JsonObject->GetField<EJson::Number>("rightThrottle");
		if(LeftThrottle->Type != EJson::Number || RightThrottle->Type != EJson::Number)
		{
			UE_LOG(LogTemp, Warning, TEXT("Received LeftThrottle or RightThrottle is not a number"));
			return;
		}
		auto LeftThrottleValue = LeftThrottle->AsNumber();
		auto RightThrottleValue = RightThrottle->AsNumber();
		// UE_LOG(LogTemp, Warning, TEXT("Received LeftThrottle: %f"), LeftThrottleValue);
		// UE_LOG(LogTemp, Warning, TEXT("Received RightThrottle: %f"), RightThrottleValue);
		// ChaosVehicleMovement->SetLeftThrottleInput(LeftThrottleValue);
		// ChaosVehicleMovement->SetRightThrottleInput(RightThrottleValue);
	});

	// Get the location and rotation of the existing scene capture component
	FVector ExistingLocation = MySceneCapture1->GetComponentLocation();
	FRotator ExistingRotation = MySceneCapture1->GetComponentRotation();

	for (int i = 0; i < 0; i++)
	{
		UCaptureManager* TempCaptureManager = NewObject<UCaptureManager>(this);
		
		// Create a new scene capture component
		USceneCaptureComponent2D* SceneCapture = NewObject<USceneCaptureComponent2D>(this);
		SceneCapture->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		SceneCapture->RegisterComponent();
		// new rotation is existing rotation plus 360 divided by the number of captures
		FRotator NewRotation = ExistingRotation + FRotator(0, 360 / 10 * i, 0);
		// FRotator NewRotation = FRotator(0, ExistingRotation.Yaw + 360 / 10 * i, 0);

		SceneCapture->SetWorldLocationAndRotation(ExistingLocation, NewRotation);

		TempCaptureManager->ColorCaptureComponents = SceneCapture;
		TempCaptureManager->SIOClientComponent = SIOClientComponent;
		TempCaptureManager->InstanceName = FString::Printf(TEXT("Instance %d"), i);
		AddInstanceComponent(TempCaptureManager);
		TempCaptureManager->RegisterComponent();
		
		CaptureManagers.Add(TempCaptureManager);
	}
	// log the number of capture managers
	// UE_LOG(LogTemp, Warning, TEXT("Number of Capture Managers: %d"), CaptureManagers.Num());
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
