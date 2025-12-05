// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDamageableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for objects that can take damage
 * Implemented by Characters, Vehicles, Destructible objects
 *
 * NOTE: For applying damage, use engine's AActor::TakeDamage() function directly.
 * This interface provides health queries and reset functionality.
 */
class FPSCORE_API IDamageableInterface
{
	GENERATED_BODY()

public:
	// Get array of components that can receive damage (for hitbox detection)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damageable")
	TArray<UPrimitiveComponent*> GetDamageableComponents();

	// Get current health value
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damageable")
	float GetHealth();

	// Get maximum health value
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damageable")
	float GetMaxHealth();

	// Check if object is dead
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damageable")
	bool IsDead();

	/**
	 * Reset character state after death (SERVER ONLY)
	 * Reverses death state: resets health, disables ragdoll, restores camera/UI
	 * Call this when respawning or reviving character
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damageable")
	void ResetAfterDeath();
};
