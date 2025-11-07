// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PlayerHUDInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UPlayerHUDInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * PlayerHUD Interface
 * Communication interface between game logic and player's HUD widget
 * Implemented by player HUD widget blueprints
 */
class FPSCORE_API IPlayerHUDInterface
{
	GENERATED_BODY()

public:
	// ============================================
	// HEALTH
	// ============================================

	// Update player health display
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Health")
	void UpdateHealth(float Health);

	// Get current health value
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Health")
	float GetHealth();

	// Add damage visual effect to HUD (red flash, vignette, etc.)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Health")
	void AddDamageEffect();

	// ============================================
	// WEAPON & AMMO
	// ============================================

	// Update active weapon display (name, icon, ammo count)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Weapon")
	void UpdateActiveWeapon(AActor* ActiveWeapon);

	// ============================================
	// CROSSHAIR
	// ============================================

	// Update crosshair based on aim state and lean angle
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Crosshair")
	void UpdateCrossHair(bool IsAim, float LeanAlpha);

	// Set crosshair widget classes for normal and aim states
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Crosshair")
	void SetCrossHair(TSubclassOf<UUserWidget> CrossHairWidgetClass, TSubclassOf<UUserWidget> AimCrossHairWidgetClass);

	// ============================================
	// UI VISIBILITY
	// ============================================

	// Set HUD visibility (show/hide entire HUD)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Visibility")
	void SetHUDVisibility(bool Visibility);

	// ============================================
	// INTERACTION
	// ============================================

	// Update item info text display (shows when looking at interactable objects)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD|Interaction")
	void UpdateItemInfo(const FString& Info);
};
