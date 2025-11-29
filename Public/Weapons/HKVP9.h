// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "HKVP9.generated.h"

class USemiAutoFireComponent;
class UBoxMagazineReloadComponent;

/**
 * HK VP9 Pistol
 * Semi-auto fire mode, 9x19mm Parabellum caliber
 *
 * SPECS:
 * - Caliber: 9x19mm Parabellum
 * - Fire Rate: 450 RPM (semi-auto limited)
 * - Magazine: 15 rounds (standard), 17 rounds (extended)
 * - Muzzle Velocity: 375 m/s
 *
 * VP9-SPECIFIC MECHANICS:
 * - Striker-fired system (no external hammer)
 * - Slide locks back when magazine empty
 * - State driven animations in weapon Anim BP
 *
 * STATE VARIABLES (read by Anim BP):
 * - bSlideLockedBack: Controls slide_locked state (empty magazine)
 *
 * ANIMATIONS:
 * - slide_locked: State anim driven by bSlideLockedBack
 * - slide_shoot: Additive montage played on each shot
 *
 * COMPONENTS (created in constructor):
 * - USemiAutoFireComponent: Semi-auto fire mode
 * - UBoxMagazineReloadComponent: Detachable box magazine reload
 * - UBallisticsComponent: Inherited from BaseWeapon
 *
 * BLUEPRINT SETUP:
 * 1. Set skeletal meshes (FPSMesh, TPSMesh)
 * 2. Set DefaultMagazineClass (BP_Magazine_VP9)
 * 3. Set DefaultSightClass (iron sights)
 * 4. Set SlideShootMontage
 * 5. Configure character animations (AnimLayer, ShootMontage, ReloadMontage)
 */
UCLASS()
class FPSCORE_API AHKVP9 : public ABaseWeapon
{
	GENERATED_BODY()

public:
	AHKVP9();

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// VP9 STATE (Replicated, Read by Anim BP)
	// ============================================

	/**
	 * Is slide locked back? (empty magazine)
	 * Controls slide_locked state in Anim BP
	 * - false: Slide forward (ready to fire)
	 * - true: Slide locked back (magazine empty)
	 * Reset to false on reload complete or unequip
	 * REPLICATED: Server authoritative, clients receive via OnRep
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SlideLockedBack, Category = "VP9|State")
	bool bSlideLockedBack = false;

protected:
	// ============================================
	// ONREP CALLBACKS
	// ============================================

	/** Called on clients when bSlideLockedBack replicates */
	UFUNCTION()
	void OnRep_SlideLockedBack();

	/**
	 * Propagate slide state to weapon mesh AnimInstances
	 * Sets bSlideLockedBack on FPSMesh and TPSMesh AnimInstances
	 * Called after state changes (server) and OnRep (clients)
	 */
	void PropagateStateToAnimInstances();

public:

	// ============================================
	// VP9 ANIMATION
	// ============================================

	/**
	 * Slide shoot montage (additive)
	 * Played on FPSMesh and TPSMesh on each shot
	 * Includes slide movement (blowback)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VP9|Animation")
	UAnimMontage* SlideShootMontage;

	// ============================================
	// BASEWEAPON OVERRIDES
	// ============================================

	/**
	 * Override Multicast_PlayShootEffects to add VP9-specific behavior
	 * - Calls base implementation (muzzle VFX + character anims)
	 * - Plays SlideShootMontage on BOTH weapon meshes (FPS + TPS)
	 * Visibility handled by mesh settings (OnlyOwnerSee/OwnerNoSee)
	 */
	virtual void Multicast_PlayShootEffects_Implementation() override;

	/**
	 * Handle shot fired - VP9 specific behavior (SERVER ONLY)
	 * - Sets bSlideLockedBack = true if magazine empty after shot
	 */
	virtual void HandleShotFired_Implementation(
		FVector_NetQuantize MuzzleLocation,
		FVector_NetQuantizeNormal Direction
	) override;

	/**
	 * Handle unequip - VP9 specific behavior
	 * - Resets bSlideLockedBack = false
	 */
	virtual void OnUnequipped_Implementation() override;

	// ============================================
	// RELOADABLE INTERFACE OVERRIDE
	// ============================================

	/**
	 * Called when reload completes - VP9 specific behavior
	 * Resets bSlideLockedBack = false (slide goes forward)
	 */
	virtual void OnWeaponReloadComplete_Implementation() override;

protected:
	// ============================================
	// COMPONENTS
	// ============================================

	// Semi-auto fire component (450 RPM max)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VP9|Components")
	USemiAutoFireComponent* SemiAutoFireComponent;

	// Box magazine reload component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VP9|Components")
	UBoxMagazineReloadComponent* BoxMagazineReloadComponent;

	// ============================================
	// HELPERS
	// ============================================

	/**
	 * Play montage on weapon meshes (FPSMesh + TPSMesh)
	 * Uses IHoldableInterface to access meshes (Golden Rule compliance)
	 * Used for slide_shoot animation
	 */
	void PlayWeaponMontage(UAnimMontage* Montage);
};
