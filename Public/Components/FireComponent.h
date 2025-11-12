// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FireComponent.generated.h"

class UBallisticsComponent;

/**
 * Fire Component (Abstract Base Class)
 * Handles fire mechanics, rate limiting, spread, recoil, and ammo consumption
 *
 * SINGLE RESPONSIBILITY: Fire mode mechanics ONLY
 *
 * DOES:
 * - Fire rate management (RPM timing)
 * - Spread application (accuracy)
 * - Recoil generation
 * - Ammo consumption (via IAmmoConsumerInterface)
 * - Trigger state management
 * - Call BallisticsComponent->Shoot()
 *
 * DOES NOT:
 * - Ballistic calculations (→ BallisticsComponent)
 * - Projectile spawning (→ BallisticsComponent)
 * - Magazine replication (→ BaseWeapon)
 * - Direct magazine access (→ Uses IAmmoConsumerInterface)
 *
 * ARCHITECTURE:
 * - Abstract base class - subclass for each fire mode
 * - NOT replicated - fire logic runs on server via RPC
 * - Decoupled from Magazine (uses IAmmoConsumerInterface)
 * - References BallisticsComponent (sibling component)
 *
 * FIRE MODES (Subclasses):
 * - USemiAutoFireComponent - One shot per trigger pull
 * - UFullAutoFireComponent - Continuous fire while trigger held
 * - UBurstFireComponent - Fixed burst count per trigger pull
 */
UCLASS(Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UFireComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFireComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// FIRE MECHANICS PROPERTIES
	// ============================================

	// Fire rate in rounds per minute (RPM)
	// Example: 600 RPM = 10 rounds/sec = 0.1s per shot
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float FireRate = 600.0f;

	// Weapon spread/accuracy (degrees)
	// Base spread cone angle - 0 = perfect accuracy, higher = more spread
	// Example: 0.5 degrees = tight grouping, 5.0 degrees = shotgun spread
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float Spread = 0.5f;

	// Recoil base angle (degrees)
	// Base vertical recoil kick per shot
	// Example: 0.5 degrees = light recoil, 2.0 degrees = heavy recoil
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float Recoil = 0.5f;

	// Random spread variation minimum (degrees)
	// Adds unpredictable variation to each shot
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float RandomSpreadMin = 0.0f;

	// Random spread variation maximum (degrees)
	// Maximum random spread variation per shot
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float RandomSpreadMax = 0.2f;

	// Movement spread multiplier
	// How much owner velocity affects spread (0 = no effect, 1.0 = full effect)
	// Formula: MovementSpread = (OwnerVelocity.Size() / 100.0) * MovementSpreadMultiplier
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float MovementSpreadMultiplier = 0.5f;

	// Recoil intensity multiplier (legacy - for camera shake)
	// Applied to camera/mesh rotation after each shot
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float RecoilScale = 1.0f;

	// ============================================
	// REFERENCES (Set by BaseWeapon)
	// ============================================

	// Ballistics component reference (sibling component)
	UPROPERTY(BlueprintReadWrite, Category = "Fire|References")
	UBallisticsComponent* BallisticsComponent = nullptr;

	// ============================================
	// FIRE CONTROL API (Called by BaseWeapon)
	// ============================================

	/**
	 * Trigger pulled (start firing)
	 * Called by BaseWeapon when player presses fire input
	 * Override in subclasses for fire mode-specific behavior
	 */
	UFUNCTION(BlueprintCallable, Category = "Fire")
	virtual void TriggerPulled();

	/**
	 * Trigger released (stop firing)
	 * Called by BaseWeapon when player releases fire input
	 * Override in subclasses for fire mode-specific behavior
	 */
	UFUNCTION(BlueprintCallable, Category = "Fire")
	virtual void TriggerReleased();

	/**
	 * Check if weapon can fire
	 * Checks: Magazine has ammo, not reloading, BallisticsComponent valid
	 */
	UFUNCTION(BlueprintPure, Category = "Fire")
	virtual bool CanFire() const;

	/**
	 * Check if trigger is currently held down
	 */
	UFUNCTION(BlueprintPure, Category = "Fire")
	bool IsTriggerHeld() const { return bTriggerHeld; }

	// ============================================
	// FIRE HELPERS
	// ============================================

	/**
	 * Apply spread to direction vector
	 * Adds random cone spread based on Spread property
	 *
	 * @param Direction - Original aim direction (normalized)
	 * @return Direction with spread applied (normalized)
	 */
	UFUNCTION(BlueprintPure, Category = "Fire")
	FVector ApplySpread(FVector Direction) const;

	/**
	 * Get time between shots in seconds
	 * Calculated from FireRate: 60.0 / FireRate
	 */
	UFUNCTION(BlueprintPure, Category = "Fire")
	float GetTimeBetweenShots() const
	{
		return FireRate > 0.0f ? 60.0f / FireRate : 0.0f;
	}

protected:
	// ============================================
	// FIRE IMPLEMENTATION (Override in subclasses)
	// ============================================

	/**
	 * Fire one shot
	 * Override in subclasses for fire mode-specific logic
	 * Base implementation:
	 * 1. Consume ammo
	 * 2. Get muzzle location and direction
	 * 3. Apply spread
	 * 4. Call BallisticsComponent->Shoot()
	 * 5. Apply recoil
	 */
	UFUNCTION(BlueprintCallable, Category = "Fire")
	virtual void Fire();

	/**
	 * Apply recoil to weapon/camera
	 * TODO: Implement recoil application
	 */
	UFUNCTION(BlueprintCallable, Category = "Fire")
	void ApplyRecoil();

	// ============================================
	// STATE
	// ============================================

	// Is trigger currently held down?
	UPROPERTY(BlueprintReadOnly, Category = "Fire|State")
	bool bTriggerHeld = false;

	// Timer handle for fire rate timing
	FTimerHandle FireRateTimer;
};
