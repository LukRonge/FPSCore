// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BallisticsComponent.h"
#include "ShotgunBallisticsComponent.generated.h"

/**
 * Shotgun Ballistics Component
 * Extends BallisticsComponent with multi-pellet shot support
 *
 * SINGLE RESPONSIBILITY: Shotgun-specific ballistics (pellet spread pattern)
 *
 * DOES:
 * - Override Shoot() to fire multiple pellets per shot
 * - Apply pellet-specific spread pattern (cone spread)
 * - Single HandleShotFired notification (one muzzle flash per shot)
 * - Multiple line traces (one per pellet)
 *
 * DOES NOT:
 * - Change fire rate (→ FireComponent)
 * - Change ammo consumption (→ FireComponent consumes 1 shell)
 * - Manage magazine (→ BaseWeapon)
 *
 * CONFIGURATION (via AmmoTypeDataAsset):
 * - PelletCount: Number of pellets per shot (default 9 for 00 buckshot)
 * - PelletSpreadAngle: Spread cone angle in degrees
 *
 * Used by: SPAS-12, Remington 870, Benelli M4, and other shotguns
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UShotgunBallisticsComponent : public UBallisticsComponent
{
	GENERATED_BODY()

public:
	UShotgunBallisticsComponent();

	// ============================================
	// SHOTGUN CONFIGURATION
	// ============================================

	/**
	 * Number of pellets per shot
	 * Default: 9 (standard 00 buckshot)
	 * Each pellet performs independent line trace and damage
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Shotgun", meta = (ClampMin = "1", ClampMax = "20"))
	int32 PelletCount = 9;

	/**
	 * Pellet spread cone angle in degrees
	 * Each pellet is randomly distributed within this cone
	 * Default: 5.0 degrees (realistic shotgun spread at close range)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Shotgun", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float PelletSpreadAngle = 5.0f;

	// ============================================
	// SHOOT API OVERRIDE
	// ============================================

	/**
	 * Shoot multiple pellets from location in direction
	 *
	 * Override of base Shoot():
	 * 1. Notify owner ONCE (single muzzle flash, single sound)
	 * 2. Fire PelletCount pellets with spread applied to each
	 * 3. Each pellet performs independent line trace and damage
	 *
	 * @param Location - Muzzle location in world space
	 * @param Direction - Base shoot direction (normalized, FireComponent spread already applied)
	 */
	virtual void Shoot(FVector Location, FVector Direction) override;

protected:
	// ============================================
	// PELLET HELPERS
	// ============================================

	/**
	 * Apply random spread to direction within cone
	 * @param Direction - Base direction (normalized)
	 * @return Direction with pellet spread applied (normalized)
	 */
	FVector ApplyPelletSpread(const FVector& Direction) const;

	/**
	 * Fire single pellet (line trace + damage + effects)
	 * Calls base class hit processing logic
	 * @param Location - Muzzle location
	 * @param Direction - Pellet direction (with spread applied)
	 */
	void ShootPellet(const FVector& Location, const FVector& Direction);
};
