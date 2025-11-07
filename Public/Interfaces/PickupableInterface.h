// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "PickupableInterface.generated.h"

class UInventoryComponent;
class UItemDataAsset;

/**
 * CAPABILITY: Pickupable
 * Interface for items that can be picked up from world and added to inventory
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
	void OnPicked(UInventoryComponent* Inventory, const FInteractionContext& Ctx);
	virtual void OnPicked_Implementation(UInventoryComponent* Inventory, const FInteractionContext& Ctx) { }

	// Called when item is dropped back to world (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickupable")
	void OnDropped(const FInteractionContext& Ctx);
	virtual void OnDropped_Implementation(const FInteractionContext& Ctx) { }

	// Get item data asset (name, icon, weight, stack size)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickupable")
	UItemDataAsset* GetItemData() const;
	virtual UItemDataAsset* GetItemData_Implementation() const { return nullptr; }

	// Get pickup sound
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickupable")
	USoundBase* GetPickupSound() const;
	virtual USoundBase* GetPickupSound_Implementation() const { return nullptr; }
};
