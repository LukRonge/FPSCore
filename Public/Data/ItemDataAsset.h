// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDataAsset.generated.h"

/**
 * Base data asset for all pickupable items
 * Contains common properties: name, icon, weight, stack size
 *
 * Replaces getters from old IPickableInterface
 */
UCLASS(BlueprintType)
class FPSCORE_API UItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Display name shown in UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	// Description text
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	// Icon for inventory UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	// World mesh when dropped
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	// Weight in kg
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	float Weight = 1.0f;

	// Maximum stack size (1 = non-stackable)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 MaxStackSize = 1;

	// Item type identifier (Weapon, Consumable, Ammo, Quest)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemType;

	// Pickup sound
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Audio")
	USoundBase* PickupSound;

	// Drop sound
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Audio")
	USoundBase* DropSound;
};
