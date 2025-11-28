// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "Sako85.generated.h"

class UBoltActionFireComponent;
class UBoltActionReloadComponent;

/**
 * Sako 85 Bolt-Action Sniper Rifle
 * Bolt-action fire mode, .308 Winchester (7.62x51mm NATO) caliber
 *
 * SPECS:
 * - Caliber: .308 Winchester / 7.62x51mm NATO
 * - Fire Rate: ~40 RPM (bolt-action limited)
 * - Magazine: 5 rounds (detachable box magazine)
 * - Muzzle Velocity: 850 m/s
 * - Effective Range: 800+ meters
 *
 * SAKO85-SPECIFIC MECHANICS:
 * - Bolt-action: Manual bolt cycling required between shots
 * - High accuracy: Minimal spread when stationary/aiming
 * - Magazine reload: Standard box magazine swap
 * - Bolt-action after reload: Chamber first round after magazine inserted
 *
 * STATE VARIABLES (read by Anim BP):
 * - bIsCyclingBolt: Controls bolt_cycling state (from BoltActionFireComponent)
 * - bChamberEmpty: Controls chamber_empty state (needs reload)
 *
 * ANIMATIONS:
 * - bolt_cycle: Played after each shot (bolt handle up, back, forward, down)
 * - reload: Standard magazine swap animation
 * - bolt_chamber: Played at end of reload to chamber first round
 *
 * COMPONENTS (created in constructor):
 * - UBoltActionFireComponent: Bolt-action fire mode
 * - UBoltActionReloadComponent: Box magazine + bolt-action reload
 * - UBallisticsComponent: Inherited from BaseWeapon
 *
 * BLUEPRINT SETUP:
 * 1. Set skeletal meshes (FPSMesh, TPSMesh)
 * 2. Set DefaultMagazineClass (BP_Magazine_308)
 * 3. Set DefaultSightClass (scope or iron sights)
 * 4. Set BoltActionMontage (character) and WeaponBoltActionMontage (weapon)
 * 5. Configure character animations (AnimLayer, ShootMontage, ReloadMontage)
 * 6. Set ReloadAttachSocket = "weapon_l" for proper hand positioning
 */
UCLASS()
class FPSCORE_API ASako85 : public ABaseWeapon
{
	GENERATED_BODY()

public:
	ASako85();

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// SAKO85 STATE (Read by Anim BP via BoltActionFireComponent)
	// ============================================
	// NOTE: State is managed by BoltActionFireComponent
	// - bIsCyclingBolt: Bolt is being cycled (blocks firing)
	// - bChamberEmpty: Chamber is empty (needs reload)
	// Access via: BoltActionFireComponent->IsCyclingBolt(), IsChamberEmpty()

	// ============================================
	// SAKO85 ANIMATION
	// ============================================

	/**
	 * Weapon-specific bolt-action montage
	 * Played on FPSMesh and TPSMesh during bolt cycling
	 * Shows bolt handle movement
	 * NOTE: Set in Blueprint or via BoltActionFireComponent->WeaponBoltActionMontage
	 */

	// ============================================
	// BASEWEAPON OVERRIDES
	// ============================================

	/**
	 * Check if weapon can be unequipped - extends base with bolt-action check
	 * Returns false if bolt is cycling or bolt-action pending after shoot
	 */
	virtual bool CanBeUnequipped_Implementation() const override;

	/**
	 * Handle unequip - Sako85 specific behavior
	 * Reset any Sako85-specific state on unequip
	 */
	virtual void OnUnequipped_Implementation() override;

	// ============================================
	// RELOADABLE INTERFACE OVERRIDE
	// ============================================

	/**
	 * Called when reload completes - Sako85 specific behavior
	 * NOTE: BoltActionReloadComponent handles bolt-action after reload
	 * This is for any additional Sako85-specific behavior
	 */
	virtual void OnWeaponReloadComplete_Implementation() override;

protected:
	// ============================================
	// COMPONENTS
	// ============================================

	// Bolt-action fire component (~40 RPM, high accuracy)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sako85|Components")
	UBoltActionFireComponent* BoltActionFireComponent;

	// Bolt-action reload component (box magazine + bolt chamber)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sako85|Components")
	UBoltActionReloadComponent* BoltActionReloadComponent;
};
