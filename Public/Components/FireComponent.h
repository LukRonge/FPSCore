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

	// Spread scale multiplier (0.0 = no spread, 1.0 = normal spread, 2.0 = double spread)
	// Applied to base spread cone angle (hardcoded base value + random variation + movement penalty)
	// Example: 0.5 = very accurate, 1.0 = normal accuracy, 2.0 = inaccurate
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Fire|Mechanics")
	float SpreadScale = 1.0f;

	// Recoil intensity multiplier (0.0 = no recoil, 1.0 = normal recoil, 2.0 = double recoil)
	// Applied to base recoil value (hardcoded)
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
	 * Applies spread cone based on SpreadScale multiplier
	 * Includes: base spread + random variation + movement penalty
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
	 * 1. Consume ammo (via IAmmoConsumerInterface)
	 * 2. Apply spread to direction (using SpreadScale)
	 * 3. Call BallisticsComponent->Shoot(Location, SpreadDirection)
	 * 4. Apply recoil (using RecoilScale)
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
