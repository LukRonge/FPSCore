// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/AmmoCaliberTypes.h"
#include "BallisticsComponent.generated.h"

class UAmmoTypeDataAsset;
class ABaseMagazine;

/**
 * Ballistics Component
 * Pure ballistic physics and projectile spawning component
 *
 * SINGLE RESPONSIBILITY: Ballistic calculations and projectile spawning ONLY
 *
 * DOES:
 * - Shoot(Location, Direction) - spawn projectile with ballistic trajectory
 * - Load CaliberDataAsset (mass, velocity, drag, damage, effects)
 * - Calculate bullet drop, kinetic energy, penetration
 * - Apply impact VFX based on surface material
 *
 * DOES NOT:
 * - Fire rate management (→ FireComponent)
 * - Spread/recoil application (→ FireComponent)
 * - Ammo consumption (→ FireComponent)
 * - Magazine management (→ BaseWeapon)
 *
 * ARCHITECTURE:
 * - NOT replicated - pure utility component
 * - Called by FireComponent after fire mechanics are processed
 * - Server authority for projectile spawning
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UBallisticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBallisticsComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// CALIBER DATA (Ballistic properties only)
	// ============================================

	// Ammo type data asset (contains ballistic properties, damage, effects)
	// This defines: Mass, Velocity, Drag, Penetration, Damage, Impact VFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ballistics")
	TSoftObjectPtr<UAmmoTypeDataAsset> CaliberDataAsset;

	// ============================================
	// SHOOT API (Core ballistics function)
	// ============================================

	/**
	 * Shoot projectile from location in direction
	 *
	 * PURE BALLISTICS - No fire rate, spread, or ammo checks
	 * Those are handled by FireComponent before calling this
	 *
	 * @param Location - Muzzle location in world space
	 * @param Direction - Shoot direction (normalized, spread already applied by FireComponent)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics")
	void Shoot(FVector Location, FVector Direction);

	// ============================================
	// BALLISTIC CALCULATIONS
	// ============================================

	/**
	 * Calculate bullet drop at given distance
	 * Uses projectile mass, velocity, and drag coefficient
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float CalculateBulletDrop(float Distance) const;

	/**
	 * Calculate kinetic energy of projectile
	 * KE = 0.5 * mass * velocity^2
	 * Used for penetration calculations
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float CalculateKineticEnergy() const;

	// ============================================
	// CALIBER DATA GETTERS (From CaliberDataAsset)
	// ============================================

	/**
	 * Get projectile mass in grams
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float GetProjectileMass() const;

	/**
	 * Get muzzle velocity in m/s
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float GetMuzzleVelocity() const;

	/**
	 * Get drag coefficient (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float GetDragCoefficient() const;

	/**
	 * Get penetration power (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float GetPenetrationPower() const;

	/**
	 * Get base damage per projectile
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float GetDamage() const;

	/**
	 * Get damage radius for explosive rounds (0 = no splash)
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	float GetDamageRadius() const;

	/**
	 * Get caliber type enum
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	EAmmoCaliberType GetCaliberType() const;

	/**
	 * Get ammo display name
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	FText GetAmmoName() const;

	/**
	 * Get ammo identifier
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	FName GetAmmoID() const;

	// ============================================
	// DEBUG
	// ============================================

	/**
	 * Log all caliber data (for debugging)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics|Debug")
	void DebugPrintCaliberData() const;
};
