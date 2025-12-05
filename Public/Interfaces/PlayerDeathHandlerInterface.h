// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayerDeathHandlerInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPlayerDeathHandlerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for handling player death notifications
 * Implemented by PlayerController to receive death events from owned Pawn
 *
 * Flow: Character dies → notifies Controller via this interface → Controller notifies GameMode
 * All operations are SERVER-SIDE only
 */
class FPSCORE_API IPlayerDeathHandlerInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called when the controlled pawn dies (SERVER ONLY)
	 * Controller should forward this to GameMode for respawn handling
	 *
	 * @param DeadPawn - The pawn that died
	 * @param Killer - The actor that caused the death (can be nullptr if unknown)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Death")
	void OnControlledPawnDeath(APawn* DeadPawn, AActor* Killer);
};
