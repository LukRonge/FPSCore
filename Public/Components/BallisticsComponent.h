// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/AmmoCaliberTypes.h"
#include "BallisticsComponent.generated.h"

class UAmmoTypeDataAsset;
class ABaseMagazine;
class UNiagaraSystem;

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
 * - Notify owner via IBallisticsHandlerInterface (shots/impacts)
 *
 * DOES NOT:
 * - Fire rate management (→ FireComponent)
 * - Spread/recoil application (→ FireComponent)
 * - Ammo consumption (→ FireComponent)
 * - Magazine management (→ BaseWeapon)
 * - Visual effects (→ BaseWeapon via IBallisticsHandlerInterface)
 *
 * ARCHITECTURE:
 * - NOT replicated - pure utility component
 * - Called by FireComponent after fire mechanics are processed
 * - Notifies owner (BaseWeapon) via IBallisticsHandlerInterface
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
	// DESIGNER DEFAULTS - CALIBER DATA
	// ============================================

	// Ammo type data asset (contains ballistic properties, damage, effects)
	// Defines: Mass, Velocity, Drag, Penetration, Damage, Impact VFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Caliber")
	TSoftObjectPtr<UAmmoTypeDataAsset> CaliberDataAsset;

	// Hash map of caliber types to ammo data assets
	// Allows quick lookup of ammo properties by caliber type
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "1 - Defaults|Caliber")
	TMap<EAmmoCaliberType, TSoftObjectPtr<UAmmoTypeDataAsset>> CaliberDataMap;

	// ============================================
	// RUNTIME STATE
	// ============================================

	// Currently active ammo type (set via InitAmmoType)
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics|Runtime")
	UAmmoTypeDataAsset* CurrentAmmoType;

	// ============================================
	// CALIBER DATA MANAGEMENT
	// ============================================

	/**
	 * Initialize ammo type from caliber type (loads from CaliberDataMap)
	 * Sets CurrentAmmoType to the loaded ammo data
	 * @param CaliberType - The caliber type to initialize
	 * @return True if caliber was found and loaded, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics")
	bool InitAmmoType(EAmmoCaliberType CaliberType);

	/**
	 * Get ammo data asset for specific caliber type
	 * @param CaliberType - The caliber type to look up
	 * @return AmmoTypeDataAsset for the caliber, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	UAmmoTypeDataAsset* GetAmmoDataForCaliber(EAmmoCaliberType CaliberType) const;

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
	virtual void Shoot(FVector Location, FVector Direction);

protected:
	// Maximum trace distance in centimeters (1000 meters)
	const float MaxTraceDistance = 100000.0f;
	/**
	 * Process single hit result
	 * Handles penetration physics, damage, effects, and impulse
	 *
	 * @param Hit - Hit result from line trace
	 * @param HitIndex - Index in hit array (0 = first hit)
	 * @param Speed - Current projectile speed (m/s) - modified by reference
	 * @param Mass - Current projectile mass (grams) - modified by reference
	 * @param Penetration - Current penetration power - modified by reference
	 * @param DropFactor - Velocity decay factor per hit
	 * @param KineticEnergy - Current kinetic energy (Joules) - modified by reference
	 * @param Direction - Shot direction for impulse calculation
	 * @param LastHitLocation - Location of previous hit (for distance calculation)
	 * @return True to continue processing hits, False to stop (bullet stopped)
	 */
	bool ProcessHit(
		const FHitResult& Hit,
		int32 HitIndex,
		float& Speed,
		float& Mass,
		float& Penetration,
		float DropFactor,
		float& KineticEnergy,
		const FVector& Direction,
		const FVector& LastHitLocation
	);

	/**
	 * Get material name and check if it's a thin material
	 * Checks if material name contains "Thin" substring
	 *
	 * Thin materials: Metal_Thin, Wood_Thin, etc. - no projectile fragmentation
	 * Solid materials: Metal, Concrete, Wood, etc. - cause projectile fragmentation
	 *
	 * @param PhysMaterial - Physical material from hit
	 * @param OutMaterialName - Output material name for VFX lookup
	 * @return True if thin material (contains "Thin"), False if solid material
	 */
	bool IsThinMaterial(UPhysicalMaterial* PhysMaterial, FName& OutMaterialName) const;

	/**
	 * Apply distance decay to kinetic energy
	 * Simulates air resistance and drag coefficient over distance
	 *
	 * @param Speed - Current projectile speed (m/s) - modified by reference
	 * @param Mass - Current projectile mass (grams) - modified by reference
	 * @param DropFactor - Drag coefficient for velocity decay
	 * @param KineticEnergy - Current kinetic energy (Joules) - modified by reference
	 * @param Distance - Distance traveled since last hit (cm)
	 */
	void ApplyDistanceDecay(
		float& Speed,
		float& Mass,
		float DropFactor,
		float& KineticEnergy,
		float Distance
	);

	/**
	 * Apply penetration loss after material impact
	 * Reduces kinetic energy based on material type (thin vs solid)
	 *
	 * @param Speed - Current projectile speed (m/s) - modified by reference
	 * @param Mass - Current projectile mass (grams) - modified by reference
	 * @param Penetration - Current penetration power - modified by reference
	 * @param DropFactor - Velocity decay factor per hit
	 * @param KineticEnergy - Current kinetic energy (Joules) - modified by reference
	 * @param PhysMaterial - Physical material that was penetrated
	 * @return True if bullet can continue, False if bullet stopped (penetration threshold reached)
	 */
	bool ApplyPenetrationLoss(
		float& Speed,
		float& Mass,
		float& Penetration,
		float DropFactor,
		float& KineticEnergy,
		UPhysicalMaterial* PhysMaterial
	);

public:

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
	// CALIBER DATA ACCESS
	// ============================================
	// NOTE: Use CurrentAmmoType directly to access caliber properties
	// All AmmoTypeDataAsset properties are BlueprintReadOnly

	/**
	 * Get current ammo type data asset
	 * Used by weapon to access ImpactVFXMap and other ammo properties
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics")
	UAmmoTypeDataAsset* GetCurrentAmmoType() const { return CurrentAmmoType; }

	// ============================================
	// DEBUG
	// ============================================

	/**
	 * Log all caliber data (for debugging)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics|Debug")
	void DebugPrintCaliberData() const;

protected:
	// ============================================
	// AMMO TYPE CACHE
	// ============================================

	/** Pre-loaded ammo type cache (loaded in constructor for CDO, inherited by instances) */
	UPROPERTY()
	TMap<EAmmoCaliberType, UAmmoTypeDataAsset*> LoadedAmmoTypes;

	/** Load all ammo types from CaliberDataMap into cache */
	void PreloadAmmoTypes();
};
