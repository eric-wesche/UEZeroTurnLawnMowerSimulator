// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mower3WheelRear.h"
#include "UObject/ConstructorHelpers.h"

UMower3WheelRear::UMower3WheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}