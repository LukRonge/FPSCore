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
 * Implement this interface on actors that need to communicate with the player's HUD
 */
class FPSCORE_API IPlayerHUDInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// Update player health display
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void UpdateHealth(float Health);

	// Get current health value
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	float GetHealth();

	// Update active weapon display
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void UpdateActiveWeapon(AActor* ActiveWeapon);

	// Add damage visual effect to HUD
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void AddDamageEffect();

	// Update crosshair based on aim state and lean angle
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void UpdateCrossHair(bool IsAim, float LeanAlpha);

	// Set crosshair widget classes for normal and aim states
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void SetCrossHair(TSubclassOf<UUserWidget> CrossHairWidgetClass, TSubclassOf<UUserWidget> AimCrossHairWidgetClass);

	// Set HUD visibility
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void SetHUDVisibility(bool Visibility);

	// Update item info text display
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PlayerHUD")
	void UpdateItemInfo(const FString& Info);
};
