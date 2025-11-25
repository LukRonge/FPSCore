// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "M4A1.generated.h"

class UFullAutoFireComponent;
class UBoxMagazineReloadComponent;

/**
 * M4A1 Assault Rifle
 * Full-auto fire mode, 5.56x45mm NATO caliber
 *
 * SPECS:
 * - Caliber: 5.56x45mm NATO
 * - Fire Rate: 800 RPM
 * - Magazine: 30 rounds STANAG
 * - Muzzle Velocity: 940 m/s
 *
 * M4A1-SPECIFIC MECHANICS:
 * - Ejection port cover: Opens on first shot, closes on unequip
 * - Bolt carrier: Locks back when magazine empty, releases on reload
 * - State driven animations in weapon Anim BP
 *
 * STATE VARIABLES (read by Anim BP):
 * - bHasFiredOnce: Controls ejection_port_cover_open state
 * - bBoltCarrierOpen: Controls bolt_carrier_open state
 *
 * ANIMATIONS:
 * - ejection_port_cover_open: State anim driven by bHasFiredOnce
 * - bolt_carrier_open: State anim driven by bBoltCarrierOpen
 * - bolt_carrier_shoot: Additive montage played on each shot
 *
 * COMPONENTS (created in constructor):
 * - UFullAutoFireComponent: Full-auto fire mode
 * - UBoxMagazineReloadComponent: Detachable box magazine reload
 * - UBallisticsComponent: Inherited from BaseWeapon
 *
 * BLUEPRINT SETUP:
 * 1. Set skeletal meshes (FPSMesh, TPSMesh)
 * 2. Set DefaultMagazineClass (BP_Magazine_STANAG)
 * 3. Set DefaultSightClass (iron sights or optic)
 * 4. Set BoltCarrierShootMontage
 * 5. Configure character animations (AnimLayer, ShootMontage, ReloadMontage)
 */
UCLASS()
class FPSCORE_API AM4A1 : public ABaseWeapon
{
	GENERATED_BODY()

public:
	AM4A1();

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// M4A1 STATE (Replicated, Read by Anim BP)
	// ============================================

	/**
	 * Has weapon fired at least once since equip?
	 * Controls ejection_port_cover_open state in Anim BP
	 * - false: Ejection port cover closed (fresh weapon)
	 * - true: Ejection port cover open (has been fired)
	 * Reset to false on unequip
	 * REPLICATED: Server authoritative, clients receive via OnRep
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HasFiredOnce, Category = "M4A1|State")
	bool bHasFiredOnce = false;

	/**
	 * Is bolt carrier locked back? (empty magazine)
	 * Controls bolt_carrier_open state in Anim BP
	 * - false: Bolt forward (ready to fire)
	 * - true: Bolt locked back (magazine empty)
	 * Reset to false on reload complete or unequip
	 * REPLICATED: Server authoritative, clients receive via OnRep
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BoltCarrierOpen, Category = "M4A1|State")
	bool bBoltCarrierOpen = false;

protected:
	// ============================================
	// ONREP CALLBACKS
	// ============================================

	/** Called on clients when bHasFiredOnce replicates */
	UFUNCTION()
	void OnRep_HasFiredOnce();

	/** Called on clients when bBoltCarrierOpen replicates */
	UFUNCTION()
	void OnRep_BoltCarrierOpen();

	/**
	 * Propagate bolt state to weapon mesh AnimInstances
	 * Sets bHasFiredOnce and bBoltCarrierOpen on FPSMesh and TPSMesh AnimInstances
	 * Called after state changes (server) and OnRep (clients)
	 */
	void PropagateStateToAnimInstances();

public:

	// ============================================
	// M4A1 ANIMATION
	// ============================================

	/**
	 * Bolt carrier shoot montage (additive)
	 * Played on FPSMesh and TPSMesh on each shot
	 * Includes bolt carrier and ejection port movement
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "M4A1|Animation")
	UAnimMontage* BoltCarrierShootMontage;

	// ============================================
	// BASEWEAPON OVERRIDES
	// ============================================

	/**
	 * Override Multicast_PlayMuzzleFlash to add M4A1-specific behavior
	 * - Calls base implementation (muzzle VFX + character anims)
	 * - Plays BoltCarrierShootMontage on weapon meshes (runs on ALL clients)
	 */
	virtual void Multicast_PlayMuzzleFlash_Implementation(
		FVector_NetQuantize MuzzleLocation,
		FVector_NetQuantizeNormal Direction
	) override;

	/**
	 * Handle shot fired - M4A1 specific behavior (SERVER ONLY)
	 * - Sets bHasFiredOnce = true on first shot
	 * - Sets bBoltCarrierOpen = true if magazine empty after shot
	 * NOTE: BoltCarrierShootMontage is played in Multicast_PlayMuzzleFlash (runs on all clients)
	 */
	virtual void HandleShotFired_Implementation(
		FVector_NetQuantize MuzzleLocation,
		FVector_NetQuantizeNormal Direction
	) override;

	/**
	 * Handle unequip - M4A1 specific behavior
	 * - Resets bHasFiredOnce = false (closes ejection port cover)
	 * - Resets bBoltCarrierOpen = false
	 */
	virtual void OnUnequipped_Implementation() override;

	// ============================================
	// RELOADABLE INTERFACE OVERRIDE
	// ============================================

	/**
	 * Called when reload completes - M4A1 specific behavior
	 * Resets bBoltCarrierOpen = false (bolt goes forward)
	 */
	virtual void OnWeaponReloadComplete_Implementation() override;

protected:
	// ============================================
	// COMPONENTS
	// ============================================

	// Full-auto fire component (800 RPM)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "M4A1|Components")
	UFullAutoFireComponent* FullAutoFireComponent;

	// Box magazine reload component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "M4A1|Components")
	UBoxMagazineReloadComponent* BoxMagazineReloadComponent;

	// ============================================
	// HELPERS
	// ============================================

	/**
	 * Play montage on weapon meshes (FPSMesh + TPSMesh)
	 * Used for bolt_carrier_shoot and other weapon-specific animations
	 */
	void PlayWeaponMontage(UAnimMontage* Montage);
};
