// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HoldableInterface.generated.h"

class UHoldableItemDataAsset;

/**
 * CAPABILITY: Holdable
 * Interface for items that can be equipped into character's hands
 *
 * Implemented by: Weapons, Tools, Throwables, Consumables
 * Design: Minimal lifecycle (equip/unequip) + data reference
 *         ALL visual data (meshes, offsets, animations) in DataAsset
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UHoldableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IHoldableInterface
{
	GENERATED_BODY()

public:
	// Called when item is equipped into character's hands
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnEquipped(APawn* Owner);
	virtual void OnEquipped_Implementation(APawn* Owner) { }

	// Called when item is unequipped (holstered or dropped)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnUnequipped();
	virtual void OnUnequipped_Implementation() { }

	// Get holdable item data asset (contains ALL visual data)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	UHoldableItemDataAsset* GetHoldableData() const;
	virtual UHoldableItemDataAsset* GetHoldableData_Implementation() const { return nullptr; }

	// Check if item is currently equipped
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool IsEquipped() const;
	virtual bool IsEquipped_Implementation() const { return false; }
};
