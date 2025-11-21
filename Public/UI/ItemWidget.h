// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemWidget.generated.h"

/**
 * Item Widget (Base Class)
 * Parent class for all item HUD widgets (weapon, grenade, medkit, etc.)
 *
 * Design:
 * - Widget holds reference to OwningItem (AActor*)
 * - Widget pulls data from item via interfaces (IAmmoConsumerInterface, etc.)
 * - Widget binds to item events for automatic updates (OnAmmoChanged, etc.)
 *
 * Subclasses:
 * - UWeaponWidget (displays weapon icon, ammo, magazine capacity)
 * - UGrenadeWidget (displays grenade icon, count)
 * - UMedkitWidget (displays medkit icon, charges)
 *
 * Usage:
 * 1. Create Blueprint child: WBP_WeaponWidget (child of UItemWidget)
 * 2. Design UI layout (icon, text, progress bars)
 * 3. Override RefreshDisplay() to update UI from OwningItem
 * 4. PlayerHUD spawns widget and calls SetOwningItem()
 */
UCLASS(Abstract, Blueprintable)
class FPSCORE_API UItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Set owning item and initialize widget
	 * Called by PlayerHUD when ActiveItem changes
	 * Binds to item events and calls RefreshDisplay()
	 *
	 * @param Item - Item actor reference (weapon, grenade, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemWidget")
	virtual void SetOwningItem(AActor* Item);

	/**
	 * Refresh widget display from item data
	 * Called when item data changes (ammo, magazine, charges, etc.)
	 * Override in Blueprint or C++ subclass to implement display logic
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemWidget")
	void RefreshDisplay();
	virtual void RefreshDisplay_Implementation();

	/**
	 * Clear widget and unbind events
	 * Called when item is removed or widget is destroyed
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemWidget")
	virtual void ClearWidget();

protected:
	/**
	 * Owning item reference (weapon, grenade, medkit, etc.)
	 * Set by SetOwningItem(), used by RefreshDisplay()
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ItemWidget")
	AActor* OwningItem = nullptr;

	/**
	 * Called when widget is destroyed
	 * Automatically clears widget and unbinds events
	 */
	virtual void NativeDestruct() override;
};
