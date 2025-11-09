// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "PickupableInterface.generated.h"

/**
 * CAPABILITY: Pickupable
 * Interface for items that can be picked up from world
 *
 * Implemented by: Weapons, Ammo boxes, Consumables, Quest items
 * Design: Query-Command pattern (CanBePicked + OnPicked)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UPickupableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IPickupableInterface
{
	GENERATED_BODY()

public:
	// Check if this item can be picked up by character
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickupable")
	bool CanBePicked(const FInteractionContext& Ctx) const;
	virtual bool CanBePicked_Implementation(const FInteractionContext& Ctx) const { return true; }

	// Called when item is picked up (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickupable")
	void OnPicked(APawn* Picker, const FInteractionContext& Ctx);
	virtual void OnPicked_Implementation(APawn* Picker, const FInteractionContext& Ctx) { }

	// Called when item is dropped back to world (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickupable")
	void OnDropped(const FInteractionContext& Ctx);
	virtual void OnDropped_Implementation(const FInteractionContext& Ctx) { }
};
