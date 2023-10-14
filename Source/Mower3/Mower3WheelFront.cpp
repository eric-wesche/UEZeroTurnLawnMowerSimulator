// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mower3WheelFront.h"
#include "UObject/ConstructorHelpers.h"

UMower3WheelFront::UMower3WheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}