// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemWidgetProviderInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UItemWidgetProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Item Widget Provider Interface
 * Implemented by items that can provide a widget class for HUD display
 *
 * Usage Pattern:
 * 1. Item implements this interface and specifies ItemWidgetClass (e.g., WBP_WeaponWidget)
 * 2. PlayerHUD calls GetItemWidgetClass() to get widget class
 * 3. PlayerHUD spawns widget and calls SetOwningItem(Item)
 * 4. Widget pulls data from item via interfaces (IAmmoConsumerInterface, etc.)
 *
 * Implementation Examples:
 * - ABaseWeapon: Returns WBP_WeaponWidget (displays ammo, magazine)
 * - ABaseGrenade: Returns WBP_GrenadeWidget (displays count)
 * - ABaseMedkit: Returns WBP_MedkitWidget (displays charges)
 */
class FPSCORE_API IItemWidgetProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get widget class for HUD display
	 * Returns widget class that will be spawned in PlayerHUD
	 *
	 * @return Widget class (UItemWidget or subclass), nullptr if no widget
	 *
	 * Example implementation (ABaseWeapon):
	 * return ItemWidgetClass; // TSubclassOf<UUserWidget> property
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemWidget")
	TSubclassOf<UUserWidget> GetItemWidgetClass() const;
	virtual TSubclassOf<UUserWidget> GetItemWidgetClass_Implementation() const { return nullptr; }
};
