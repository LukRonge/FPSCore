// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "Spas12.generated.h"

class USemiAutoFireComponent;
class UPumpActionReloadComponent;

/**
 * SPAS-12 Combat Shotgun
 * Semi-auto fire mode, 12 Gauge caliber
 *
 * SPECS:
 * - Caliber: 12 Gauge (00 Buckshot)
 * - Fire Rate: ~300 RPM (semi-auto, one shot per trigger pull)
 * - Magazine: 8 rounds (tube magazine)
 * - Pellet Count: 9 pellets per shot
 * - Effective Range: 40 meters
 *
 * SPAS-12-SPECIFIC MECHANICS:
 * - Semi-auto firing: One shot per trigger pull, no pump-action during shooting
 * - Shell-by-shell reload: Press IA_Reload to insert one shell at a time
 * - Pump-action after reload: Only when chamber was empty (chambers first round)
 * - Reload interrupt: Can fire or stop reloading at any time
 * - Bolt carrier: Locks back when magazine empty, releases on reload
 *
 * STATE VARIABLES (read by Anim BP):
 * - bChamberEmpty: Controls chamber_empty state (needs reload)
 *   Managed by PumpActionReloadComponent
 * - BoltCarrierOpen: Controls bolt_carrier_open state (magazine empty)
 *
 * ANIMATIONS:
 * - shoot: Recoil animation (standard, no pump-action follow-up)
 * - bolt_carrier_shoot: Additive montage played on each shot
 * - reload: Single shell insertion animation (per shell)
 * - reload_pump: Pump-action after first shell if chamber was empty
 *
 * COMPONENTS (created in constructor):
 * - USemiAutoFireComponent: Semi-auto fire mode (one shot per click)
 * - UPumpActionReloadComponent: Shell-by-shell reload with pump-action when needed
 * - UShotgunBallisticsComponent: Multi-pellet ballistics (9 pellets per shot)
 *
 * BLUEPRINT SETUP:
 * 1. Set skeletal meshes (FPSMesh, TPSMesh)
 * 2. Set DefaultMagazineClass (BP_Magazine_12GaugeShell)
 * 3. Set DefaultSightClass (iron sights)
 * 4. Set BoltCarrierShootMontage for weapon mesh animation
 * 5. Set ReloadMontage on PumpActionReloadComponent
 * 6. Configure character animations (AnimLayer, ShootMontage, ReloadMontage)
 * 7. Set ReloadAttachSocket = "weapon_l" for proper hand positioning during reload
 */
UCLASS()
class FPSCORE_API ASpas12 : public ABaseWeapon
{
	GENERATED_BODY()

public:
	ASpas12();

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// SPAS-12 STATE (Replicated, Read by Anim BP)
	// ============================================

	/**
	 * Is bolt carrier locked back? (empty magazine)
	 * Controls bolt_carrier_open state in Anim BP
	 * - false: Bolt forward (ready to fire)
	 * - true: Bolt locked back (magazine empty)
	 * Reset to false on reload complete or unequip
	 * REPLICATED: Server authoritative, clients receive via OnRep
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BoltCarrierOpen, Category = "Spas12|State")
	bool BoltCarrierOpen = false;

protected:
	// ============================================
	// ONREP CALLBACKS
	// ============================================

	/** Called on clients when BoltCarrierOpen replicates */
	UFUNCTION()
	void OnRep_BoltCarrierOpen();

	// NOTE: Uses BaseWeapon::ForceUpdateWeaponAnimInstances() for AnimInstance updates

public:
	// ============================================
	// SPAS-12 ANIMATION
	// ============================================

	/**
	 * Bolt carrier shoot montage (additive)
	 * Played on FPSMesh and TPSMesh on each shot
	 * Shows bolt carrier cycling during semi-auto fire
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spas12|Animation")
	UAnimMontage* BoltCarrierShootMontage = nullptr;

	// ============================================
	// BASEWEAPON OVERRIDES
	// ============================================

	/**
	 * Override Multicast_PlayShootEffects to add SPAS-12-specific behavior
	 * - Calls base implementation (muzzle VFX + character anims)
	 * - Plays BoltCarrierShootMontage on weapon meshes (runs on ALL clients)
	 */
	virtual void Multicast_PlayShootEffects_Implementation() override;

	/**
	 * Handle shot fired - SPAS-12 specific behavior (SERVER ONLY)
	 * - Sets BoltCarrierOpen = true if magazine empty after shot
	 * NOTE: BoltCarrierShootMontage is played in Multicast_PlayShootEffects (runs on all clients)
	 */
	virtual void HandleShotFired_Implementation(
		FVector_NetQuantize MuzzleLocation,
		FVector_NetQuantizeNormal Direction
	) override;

	/**
	 * Handle unequip - SPAS-12 specific behavior
	 * - Resets BoltCarrierOpen = false
	 * - Resets PumpActionReloadComponent chamber state
	 */
	virtual void OnUnequipped_Implementation() override;

	// ============================================
	// RELOADABLE INTERFACE OVERRIDE
	// ============================================

	/**
	 * Called when reload completes - SPAS-12 specific behavior
	 * Resets BoltCarrierOpen = false (bolt goes forward)
	 * NOTE: PumpActionReloadComponent handles pump-action after reload
	 */
	virtual void OnWeaponReloadComplete_Implementation() override;

protected:
	// ============================================
	// COMPONENTS
	// ============================================

	// Semi-auto fire component (~300 RPM, high spread)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spas12|Components")
	USemiAutoFireComponent* SemiAutoFireComponent;

	// Pump-action reload component (shell-by-shell reload)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spas12|Components")
	UPumpActionReloadComponent* PumpActionReloadComponent;

	// NOTE: Uses BaseWeapon::PlayWeaponMontage() for weapon mesh animations
};
