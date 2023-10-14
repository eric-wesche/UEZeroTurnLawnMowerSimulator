// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mower3Pawn.h"
#include "Mower3SportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class MOWER3_API AMower3SportsCar : public AMower3Pawn
{
	GENERATED_BODY()
	
public:

	AMower3SportsCar(const FObjectInitializer& ObjectInitializer);
};
