// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mower3GameMode.h"
#include "Mower3PlayerController.h"

AMower3GameMode::AMower3GameMode()
{
	PlayerControllerClass = AMower3PlayerController::StaticClass();
}
