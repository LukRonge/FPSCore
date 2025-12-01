// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ProjectileInterface.generated.h"

/**
 * CAPABILITY: Projectile
 * Interface for projectile actors that can be initialized with velocity.
 *
 * Implemented by: AGrenadeProjectile, ARocketProjectile, etc.
 * Design: Provides projectile initialization without direct class casts
 *
 * Used by: ABaseGrenade, AM72A7_Law to initialize spawned projectiles
 * via interface call instead of direct method call.
 *
 * Why this interface?
 * - Golden Rule: NEVER use direct class references for communication
 * - Allows different projectile types (grenade, rocket, knife)
 * - Enables loose coupling between thrower and projectile
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UProjectileInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IProjectileInterface
{
	GENERATED_BODY()

public:
	/**
	 * Initialize projectile after spawn
	 * Called by spawning actor after SpawnActor
	 * Projectile uses its rotation and ProjectileMovementComponent settings
	 *
	 * @param InInstigator - Pawn that launched this projectile (for damage attribution)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(APawn* InInstigator);
	virtual void InitializeProjectile_Implementation(APawn* InInstigator) { }

	/**
	 * Check if projectile is still active (not exploded/destroyed)
	 * @return true if projectile is active
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Projectile")
	bool IsProjectileActive() const;
	virtual bool IsProjectileActive_Implementation() const { return true; }
};
