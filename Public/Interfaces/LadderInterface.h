// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LadderInterface.generated.h"

class AFPSCharacter;

UINTERFACE(MinimalAPI, Blueprintable)
class ULadderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for ladders and climbable objects
 * Extends IInteractableInterface
 * Implemented by BP_Ladder_Parent
 */
class FPSCORE_API ILadderInterface
{
	GENERATED_BODY()

public:
	// Start using the ladder
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ladder")
	void UseLadder(AFPSCharacter* Character);

	// Get bottom entry point transform
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ladder")
	FTransform GetLadderBottomStart();

	// Get bottom exit point transform
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ladder")
	FTransform GetLadderBottomExit();

	// Get top entry point transform
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ladder")
	FTransform GetLadderTopStart();

	// Get top exit point transform
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ladder")
	FTransform GetLadderTopExit();
};
