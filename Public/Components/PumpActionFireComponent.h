// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/FireComponent.h"
#include "PumpActionFireComponent.generated.h"

/**
 * Pump-Action Fire Component
 * Single shot followed by pump-action cycling sequence
 *
 * FIRE MODE: Pump-Action (supports both semi-auto and full-auto trigger modes)
 *
 * BEHAVIOR:
 * - TriggerPulled() → Fire one shot, mark pump-action as pending
 * - ShootMontage plays (recoil animation)
 * - ShootMontage ends → OnShootMontageEnded() → StartPumpAction()
 * - Pump-action montage plays (shell ejection, chamber next round)
 * - Cannot fire again until pump-action completes
 * - TriggerReleased() → Reset trigger state
 *
 * PUMP-ACTION SEQUENCE (after shoot montage):
 * 1. Fire() → Shot fired, bPumpActionPendingAfterShoot = true
 * 2. ShootMontage ends → OnShootMontageEnded()
 * 3. StartPumpAction() → PumpActionMontage starts
 * 4. AnimNotify_PumpActionStart → WeaponPumpActionMontage starts (synchronized with hand movement)
 * 5. AnimNotify_ShellEject → Shell casing ejected
 * 6. AnimNotify_ChamberRound → Next round chambered (if ammo available)
 * 7. PumpActionMontage ends → bIsPumping = false, can fire again
 *
 * STATE:
 * - bIsPumping: True during pump-action animation, blocks firing
 * - bChamberEmpty: True if no round in chamber (must reload)
 * - bPumpActionPendingAfterShoot: True after fire, waiting for shoot montage to end
 *
 * FIRE MODES:
 * - bFullAutoTrigger = false: One shot per trigger pull (pump-action shotguns like Remington 870)
 * - bFullAutoTrigger = true: Continuous fire while trigger held (SPAS-12 semi-auto mode, fires as fast as pump cycles)
 *
 * USE CASES:
 * - Pump-action shotguns (Remington 870, Mossberg 500, SPAS-12)
 *
 * NOTE: This component handles fire blocking during pump-action.
 * PumpActionReloadComponent handles the shell-by-shell reload sequence.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UPumpActionFireComponent : public UFireComponent
{
	GENERATED_BODY()

public:
	UPumpActionFireComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// DESIGNER DEFAULTS - FIRE MODE
	// ============================================

	// Enable full-auto trigger mode
	// false = one shot per trigger pull (traditional pump-action)
	// true = continuous fire while trigger held (fires as fast as pump cycles)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Fire")
	bool bFullAutoTrigger = false;

	// ============================================
	// DESIGNER DEFAULTS - ANIMATION
	// ============================================

	// Pump-action montage for character (Body/Arms/Legs)
	// Played after each shot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "1 - Defaults|Animation")
	UAnimMontage* PumpActionMontage = nullptr;

	// Pump-action montage for weapon mesh (FPS + TPS)
	// Shows pump handle movement, shell ejection
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "1 - Defaults|Animation")
	UAnimMontage* WeaponPumpActionMontage = nullptr;

	// ============================================
	// RUNTIME STATE (REPLICATED)
	// ============================================

	// Is pump currently cycling? (REPLICATED) - blocks firing
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsPumping, Category = "PumpAction|Runtime")
	bool bIsPumping = false;

	// Is chamber empty? (REPLICATED) - needs reload
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ChamberEmpty, Category = "PumpAction|Runtime")
	bool bChamberEmpty = false;

protected:
	UFUNCTION()
	void OnRep_IsPumping();

	UFUNCTION()
	void OnRep_ChamberEmpty();

	/**
	 * Propagate pump state to weapon mesh AnimInstances
	 * Called after state changes (server) and OnRep (clients)
	 */
	void PropagateStateToAnimInstances();

public:
	// ============================================
	// FIRE CONTROL OVERRIDE
	// ============================================

	/**
	 * Check if weapon can fire
	 * Extends base check with pump-action specific conditions
	 * Returns false if: pumping, chamber empty, or base conditions fail
	 */
	virtual bool CanFire() const override;

	/**
	 * Trigger pulled - fire one shot if conditions met
	 * After firing, starts pump-action sequence
	 */
	virtual void TriggerPulled() override;

	/**
	 * Trigger released - allow next shot attempt
	 */
	virtual void TriggerReleased() override;

	// ============================================
	// PUMP-ACTION API (Called by AnimNotifies and ReloadComponent)
	// ============================================

	/**
	 * Start pump-action cycling sequence
	 * Called after shot fired or manually if needed
	 * Plays PumpActionMontage and sets bIsPumping = true
	 *
	 * @param bFromReload - True if called from reload completion (chambers first round)
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void StartPumpAction(bool bFromReload = false);

	/**
	 * Called when pump-action montage completes
	 * Resets bIsPumping = false, allowing next shot
	 * SERVER ONLY for state changes, runs on all machines for local cleanup
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void OnPumpActionComplete();

	/**
	 * Called by AnimNotify_PumpActionStart during character pump-action montage
	 * Starts weapon pump-action montage (WeaponPumpActionMontage)
	 * Synchronizes weapon pump animation with character hand movement
	 * LOCAL operation (runs on all machines)
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void OnPumpActionStart();

	/**
	 * Called by AnimNotify_ShellEject during pump-action montage
	 * Spawns shell casing VFX at ejection port
	 * LOCAL operation (runs on all machines)
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void OnShellEject();

	/**
	 * Called by AnimNotify_ChamberRound during pump-action montage
	 * Chambers next round from magazine (if available)
	 * If magazine empty, sets bChamberEmpty = true
	 * LOCAL operation (visual), SERVER handles state
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void OnChamberRound();

	/**
	 * Check if pump-action is currently in progress
	 */
	UFUNCTION(BlueprintPure, Category = "PumpAction")
	bool IsPumping() const { return bIsPumping; }

	/**
	 * Check if chamber is empty (needs reload)
	 */
	UFUNCTION(BlueprintPure, Category = "PumpAction")
	bool IsChamberEmpty() const { return bChamberEmpty; }

	/**
	 * Reset chamber state (called after reload)
	 * Sets bChamberEmpty = false
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void ResetChamberState();

	/**
	 * Called when shoot montage ends - triggers pump-action sequence
	 * Should be called by weapon class after ShootMontage completes
	 * SERVER: Starts pump-action if bPumpActionPendingAfterShoot is true
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void OnShootMontageEnded();

	/**
	 * Check if pump-action is pending after shoot
	 * Used by weapon class to know if it should register shoot montage delegate
	 */
	UFUNCTION(BlueprintPure, Category = "PumpAction")
	bool IsPumpActionPendingAfterShoot() const { return bPumpActionPendingAfterShoot; }

	/**
	 * Set pump-action state for reload integration (SERVER ONLY)
	 * Called by PumpActionReloadComponent when triggering pump-action after reload
	 * Encapsulates state modification - external components should use this instead of direct property access
	 *
	 * @param bPumping - Set bIsPumping state
	 * @param bChamberIsEmpty - Set bChamberEmpty state
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void SetPumpActionState(bool bPumping, bool bChamberIsEmpty);

protected:
	/**
	 * Is pump-action pending after shoot? (waiting for shoot montage to end)
	 * Set to true after Fire(), reset when OnShootMontageEnded() is called
	 * NOT replicated - server-only state for timing control
	 */
	bool bPumpActionPendingAfterShoot = false;

	/**
	 * Should fire again after pump-action completes? (full-auto mode)
	 * Set when trigger is held and bFullAutoTrigger is true
	 */
	bool bWantsToFireAfterPump = false;

public:
	// ============================================
	// INTERNAL HELPERS
	// ============================================

	/**
	 * Play pump-action montages on character and weapon meshes
	 * LOCAL operation - each machine plays independently
	 */
	void PlayPumpActionMontages();

	/**
	 * Stop pump-action montages (if interrupted)
	 * LOCAL operation
	 */
	void StopPumpActionMontages();

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
	 * Used during pump-action to move weapon from weapon_r to weapon_l and back
	 * @param bToReloadSocket - true: attach to ReloadAttachSocket (weapon_l), false: attach to CharacterAttachSocket (weapon_r)
	 */
	void ReattachWeaponToSocket(bool bToReloadSocket);

	/**
	 * Reattach weapon to specific socket on character (overload)
	 * Handles both FPS (Arms) and TPS (Body) mesh attachments
	 *
	 * @param SocketName - Socket name on character mesh to attach weapon to
	 */
	void ReattachWeaponToSocket(FName SocketName);
};
