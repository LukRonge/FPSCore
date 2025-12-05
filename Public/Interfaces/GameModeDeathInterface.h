// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameModeDeathInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UGameModeDeathInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for handling player death notifications in GameMode
 * Implemented by GameMode to receive death events from PlayerController
 *
 * Flow: Character dies → Controller receives → GameMode handles via this interface
 * All operations are SERVER-SIDE only (GameMode only exists on server)
 */
class FPSCORE_API IGameModeDeathInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called when a player's pawn dies (SERVER ONLY)
	 * GameMode should handle respawn logic, scoring, match state, etc.
	 *
	 * @param PlayerController - The controller that owned the dead pawn
	 * @param DeadPawn - The pawn that died
	 * @param Killer - The actor that caused the death (can be nullptr if unknown)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Death")
	void OnPlayerDeath(AController* PlayerController, APawn* DeadPawn, AActor* Killer);
};
