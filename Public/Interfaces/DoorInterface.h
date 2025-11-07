// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DoorInterface.generated.h"

class AFPSCharacter;

UINTERFACE(MinimalAPI, Blueprintable)
class UDoorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for doors and openable objects
 * Extends IInteractableInterface
 * Implemented by BP_Door_Parent
 */
class FPSCORE_API IDoorInterface
{
	GENERATED_BODY()

public:
	// Open the door
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Door")
	void Open(AFPSCharacter* Character);

	// Close the door
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Door")
	void Close();

	// Toggle door state (open/close)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Door")
	void Toggle();

	// Check if door is locked
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Door")
	bool IsLocked();

	// Set door locked state
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Door")
	void SetLocked(bool bLocked);
};
