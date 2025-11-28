// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/FireComponent.h"
#include "BoltActionFireComponent.generated.h"

/**
 * Bolt-Action Fire Component
 * Single shot per trigger, requires manual bolt cycling before next shot
 *
 * FIRE MODE: Bolt-Action
 *
 * BEHAVIOR:
 * - TriggerPulled() → Fire one shot, mark bolt-action as pending
 * - ShootMontage plays (recoil animation)
 * - ShootMontage ends → OnShootMontageEnded() → StartBoltAction()
 * - Bolt-action montage plays (shell ejection, chamber next round)
 * - Cannot fire again until bolt-action completes
 * - TriggerReleased() → Reset trigger state
 *
 * BOLT-ACTION SEQUENCE (after shoot montage):
 * 1. Fire() → Shot fired, bBoltActionPendingAfterShoot = true
 * 2. ShootMontage ends → OnShootMontageEnded()
 * 3. StartBoltAction() → BoltActionMontage starts
 * 4. AnimNotify_ShellEject → Shell casing ejected (if ammo available)
 * 5. AnimNotify_ChamberRound → Next round chambered (if ammo available)
 * 6. BoltActionMontage ends → bIsCyclingBolt = false, can fire again
 *
 * STATE:
 * - bIsCyclingBolt: True during bolt-action animation, blocks firing
 * - bChamberEmpty: True if no round in chamber (must reload)
 * - bBoltActionPendingAfterShoot: True after fire, waiting for shoot montage to end
 *
 * USE CASES:
 * - Bolt-action sniper rifles (Sako 85, Remington 700, AWM)
 * - Bolt-action hunting rifles
 *
 * NOTE: This component handles fire blocking during bolt-action.
 * BoltActionReloadComponent handles the reload sequence which also
 * triggers bolt-action at the end to chamber first round.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UBoltActionFireComponent : public UFireComponent
{
	GENERATED_BODY()

public:
	UBoltActionFireComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// BOLT-ACTION STATE (Replicated)
	// ============================================

	/**
	 * Is bolt currently being cycled? (REPLICATED)
	 * True from shot fired until bolt-action montage completes
	 * Blocks firing when true
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsCyclingBolt, Category = "BoltAction|State")
	bool bIsCyclingBolt = false;

	/**
	 * Is chamber empty? (needs reload) (REPLICATED)
	 * True when magazine empty and bolt cycled (no round to chamber)
	 * Reset to false after successful reload + bolt-action
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ChamberEmpty, Category = "BoltAction|State")
	bool bChamberEmpty = false;

protected:
	UFUNCTION()
	void OnRep_IsCyclingBolt();

	UFUNCTION()
	void OnRep_ChamberEmpty();

	/**
	 * Propagate bolt state to weapon mesh AnimInstances
	 * Called after state changes (server) and OnRep (clients)
	 */
	void PropagateStateToAnimInstances();

public:
	// ============================================
	// BOLT-ACTION ANIMATION
	// ============================================

	/**
	 * Bolt-action animation montage
	 * Played on character Body/Arms/Legs meshes after each shot
	 * Contains AnimNotify_ShellEject and AnimNotify_ChamberRound
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BoltAction|Animation")
	UAnimMontage* BoltActionMontage = nullptr;

	/**
	 * Bolt-action animation montage for weapon mesh
	 * Played on FPSMesh and TPSMesh simultaneously with character montage
	 * Shows bolt handle movement, shell ejection port opening, etc.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BoltAction|Animation")
	UAnimMontage* WeaponBoltActionMontage = nullptr;

	// ============================================
	// FIRE CONTROL OVERRIDE
	// ============================================

	/**
	 * Check if weapon can fire
	 * Extends base check with bolt-action specific conditions
	 * Returns false if: cycling bolt, chamber empty, or base conditions fail
	 */
	virtual bool CanFire() const override;

	/**
	 * Trigger pulled - fire one shot if conditions met
	 * After firing, starts bolt-action sequence
	 */
	virtual void TriggerPulled() override;

	/**
	 * Trigger released - allow next shot attempt
	 */
	virtual void TriggerReleased() override;

	// ============================================
	// BOLT-ACTION API (Called by AnimNotifies and ReloadComponent)
	// ============================================

	/**
	 * Start bolt-action cycling sequence
	 * Called after shot fired or at end of reload
	 * Plays BoltActionMontage and sets bIsCyclingBolt = true
	 *
	 * @param bFromReload - True if called from reload completion (chambers first round)
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void StartBoltAction(bool bFromReload = false);

	/**
	 * Called when bolt-action montage completes
	 * Resets bIsCyclingBolt = false, allowing next shot
	 * SERVER ONLY for state changes, runs on all machines for local cleanup
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void OnBoltActionComplete();

	/**
	 * Called by AnimNotify_BoltActionStart during character bolt-action montage
	 * Starts weapon bolt-action montage (WeaponBoltActionMontage)
	 * Synchronizes weapon bolt animation with character hand movement
	 * LOCAL operation (runs on all machines)
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void OnBoltActionStart();

	/**
	 * Called by AnimNotify_ShellEject during bolt-action montage
	 * Spawns shell casing VFX at ejection port
	 * LOCAL operation (runs on all machines)
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void OnShellEject();

	/**
	 * Called by AnimNotify_ChamberRound during bolt-action montage
	 * Chambers next round from magazine (if available)
	 * If magazine empty, sets bChamberEmpty = true
	 * LOCAL operation (visual), SERVER handles state
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void OnChamberRound();

	/**
	 * Check if bolt-action is currently in progress
	 */
	UFUNCTION(BlueprintPure, Category = "BoltAction")
	bool IsCyclingBolt() const { return bIsCyclingBolt; }

	/**
	 * Check if chamber is empty (needs reload)
	 */
	UFUNCTION(BlueprintPure, Category = "BoltAction")
	bool IsChamberEmpty() const { return bChamberEmpty; }

	/**
	 * Reset chamber state (called after reload)
	 * Sets bChamberEmpty = false
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void ResetChamberState();

	/**
	 * Called when shoot montage ends - triggers bolt-action sequence
	 * Should be called by weapon class (e.g., Sako85) after ShootMontage completes
	 * SERVER: Starts bolt-action if bBoltActionPendingAfterShoot is true
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void OnShootMontageEnded();

	/**
	 * Check if bolt-action is pending after shoot
	 * Used by weapon class to know if it should register shoot montage delegate
	 */
	UFUNCTION(BlueprintPure, Category = "BoltAction")
	bool IsBoltActionPendingAfterShoot() const { return bBoltActionPendingAfterShoot; }

	/**
	 * Set bolt-action state for reload integration (SERVER ONLY)
	 * Called by BoltActionReloadComponent when triggering bolt-action after reload
	 * Encapsulates state modification - external components should use this instead of direct property access
	 *
	 * @param bCycling - Set bIsCyclingBolt state
	 * @param bChamberIsEmpty - Set bChamberEmpty state
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction")
	void SetBoltActionState(bool bCycling, bool bChamberIsEmpty);

protected:
	/**
	 * Is bolt-action pending after shoot? (waiting for shoot montage to end)
	 * Set to true after Fire(), reset when OnShootMontageEnded() is called
	 * NOT replicated - server-only state for timing control
	 */
	bool bBoltActionPendingAfterShoot = false;

public:
	// ============================================
	// INTERNAL HELPERS
	// ============================================

	/**
	 * Play bolt-action montages on character and weapon meshes
	 * LOCAL operation - each machine plays independently
	 */
	void PlayBoltActionMontages();

	/**
	 * Stop bolt-action montages (if interrupted)
	 * LOCAL operation
	 */
	void StopBoltActionMontages();

	/**
	 * Montage ended callback
	 * @param Montage - The montage that ended
	 * @param bInterrupted - True if montage was interrupted
	 */
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Play montage on weapon FPS and TPS meshes
	 * @param Montage - Montage to play
	 */
	void PlayWeaponMontage(UAnimMontage* Montage);

	/**
	 * Stop montage on weapon FPS and TPS meshes
	 * @param Montage - Montage to stop
	 */
	void StopWeaponMontage(UAnimMontage* Montage);

	/**
	 * Reattach weapon to socket on character
	 * Used during bolt-action to move weapon from weapon_r to weapon_l and back
	 * @param bToReloadSocket - true: attach to ReloadAttachSocket (weapon_l), false: attach to CharacterAttachSocket (weapon_r)
	 */
	void ReattachWeaponToSocket(bool bToReloadSocket);

	/**
	 * Reattach weapon to specific socket on character (overload)
	 * Shared implementation used by both BoltActionFireComponent and BoltActionReloadComponent
	 * Handles both FPS (Arms) and TPS (Body) mesh attachments
	 *
	 * @param SocketName - Socket name on character mesh to attach weapon to
	 */
	void ReattachWeaponToSocket(FName SocketName);
};
