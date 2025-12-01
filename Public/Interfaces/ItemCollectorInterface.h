// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemCollectorInterface.generated.h"

/**
 * CAPABILITY: Item Collector
 * Interface for actors that can pickup and drop items
 *
 * Implemented by: AFPSCharacter, AI characters, vehicles
 * Design: Actors with this capability can collect items from world
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UItemCollectorInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IItemCollectorInterface
{
	GENERATED_BODY()

public:
	/**
	 * Pickup item from world
	 * @param Item - Item to pickup
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	void Pickup(AActor* Item);
	virtual void Pickup_Implementation(AActor* Item) { }

	/**
	 * Drop item to world
	 * @param Item - Item to drop
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	void Drop(AActor* Item);
	virtual void Drop_Implementation(AActor* Item) { }

	/**
	 * Get currently active/equipped item
	 * @return Active item or nullptr if no item equipped
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	AActor* GetActiveItem() const;
	virtual AActor* GetActiveItem_Implementation() const { return nullptr; }

	/**
	 * Get item currently being unequipped (during unequip montage)
	 * Used by AnimNotify_ItemUnequipStart to trigger item's collapse animation
	 * @return Item being unequipped or nullptr if not in unequip state
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	AActor* GetUnequippingItem() const;
	virtual AActor* GetUnequippingItem_Implementation() const { return nullptr; }

	// ============================================
	// EQUIP/UNEQUIP MONTAGE CALLBACKS
	// ============================================

	/**
	 * Called when unequip montage finishes (from AnimNotify_UnequipFinished)
	 * Handles holstering current item and starting pending equip if switching
	 * LOCAL operation - runs on all machines independently
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	void OnUnequipMontageFinished();
	virtual void OnUnequipMontageFinished_Implementation() { }
};
