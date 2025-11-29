// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ReloadComponent.h"
#include "PumpActionReloadComponent.generated.h"

/**
 * UPumpActionReloadComponent
 *
 * Shotgun shell-by-shell reload implementation.
 * Works with any fire component (SemiAuto, PumpAction, etc.)
 * Uses weapon's Magazine as the shell visual (no separate shell mesh spawning).
 *
 * RELOAD BEHAVIOR:
 * - Single IA_Reload press = insert ONE shell
 * - Reload stops when magazine is full (MaxCapacity)
 * - Reload can be interrupted at any time (fire, unequip, sprint)
 * - Player can continue reloading by pressing IA_Reload again
 *
 * SHELL VISUAL SYSTEM:
 * - Uses weapon's MagazineComponent as the shell mesh
 * - Magazine mesh is hidden by default (shells are in tube, not visible)
 * - OnGrabShell: Show magazine mesh, attach to character hand
 * - OnShellInsert: Hide magazine mesh, reattach to weapon
 * - This reuses existing FPS/TPS visibility system from Magazine
 *
 * RELOAD SEQUENCE (per shell):
 * 1. Weapon re-attaches to ReloadAttachSocket (e.g., weapon_l for left hand support)
 * 2. Reload montage starts (Body/Arms/Legs)
 * 3. AnimNotify_GrabShell fires → OnGrabShell()
 *    - Show magazine mesh, attach to character hand socket
 * 4. AnimNotify_ShellInsert fires → OnShellInsert()
 *    - SERVER: Add 1 ammo to magazine
 *    - LOCAL: Hide magazine mesh, reattach to weapon
 * 5. Montage completes → OnReloadComplete()
 *    - Weapon re-attaches back to CharacterAttachSocket
 *    - bIsReloading = false (ready for next shell or firing)
 *
 * PUMP-ACTION AFTER RELOAD (if chamber was empty):
 * - If bChamberEmpty was true when reload started
 * - After first shell inserted, pump-action is triggered via AnimNotify
 * - This chambers the first round from the tube
 * - Subsequent shells don't require pump-action
 *
 * ANIMATION NOTIFIES:
 * - AnimNotify_GrabShell: Show magazine (shell) in hand
 * - AnimNotify_ShellInsert: Hide magazine, +1 ammo (SERVER)
 * - AnimNotify_ReloadPumpAction: Trigger pump-action (only for first shell if chamber empty)
 *
 * Used by: Shotguns with shell-by-shell reload (Remington 870, Mossberg 500, SPAS-12)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UPumpActionReloadComponent : public UReloadComponent
{
	GENERATED_BODY()

public:
	UPumpActionReloadComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// CONFIGURATION
	// ============================================

	/**
	 * Should weapon re-attach to ReloadAttachSocket during reload?
	 * If true, weapon moves from CharacterAttachSocket to ReloadAttachSocket
	 * at reload start, and back to CharacterAttachSocket after reload completes
	 * Default: true (shotguns typically need hand repositioning)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PumpAction|Reload")
	bool bReattachDuringReload = true;

	/**
	 * Socket name on character mesh where shell (magazine) attaches during grab
	 * Shell is grabbed by hand, then inserted into weapon
	 * Default: "magazine_l" (left hand magazine socket, same as box magazine reload)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PumpAction|Shell")
	FName ShellGrabSocketName = FName("magazine_l");

	// ============================================
	// PUMP-ACTION ANIMATION (for reload pump)
	// ============================================

	/**
	 * Pump-action animation montage
	 * Played on character Body/Arms/Legs meshes after first shell if chamber was empty
	 * Contains AnimNotify_PumpActionStart for weapon sync
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PumpAction|Animation")
	UAnimMontage* PumpActionMontage = nullptr;

	/**
	 * Pump-action animation montage for weapon mesh
	 * Played on FPSMesh and TPSMesh simultaneously with character montage
	 * Shows pump handle movement
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PumpAction|Animation")
	UAnimMontage* WeaponPumpActionMontage = nullptr;

	// ============================================
	// REPLICATED STATE
	// ============================================

	/**
	 * Is chamber empty? (needs reload + pump-action) (REPLICATED)
	 * True when magazine empty and last round fired
	 * Reset to false after pump-action chambers a round
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ChamberEmpty, Category = "PumpAction|State")
	bool bChamberEmpty = false;

	/**
	 * Is pump currently being cycled? (REPLICATED)
	 * True during pump-action montage after reload
	 * Blocks firing and reloading when true
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsPumping, Category = "PumpAction|State")
	bool bIsPumping = false;

	/**
	 * Was chamber empty when reload started? (REPLICATED)
	 * If true, first shell insertion will trigger pump-action to chamber round
	 * Reset to false after pump-action completes
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_NeedsPumpAfterReload, Category = "PumpAction|State")
	bool bNeedsPumpAfterReload = false;

protected:
	UFUNCTION()
	void OnRep_ChamberEmpty();

	UFUNCTION()
	void OnRep_IsPumping();

	UFUNCTION()
	void OnRep_NeedsPumpAfterReload();

public:
	// ============================================
	// STATE API
	// ============================================

	/**
	 * Check if pump is currently cycling
	 */
	UFUNCTION(BlueprintPure, Category = "PumpAction")
	bool IsPumping() const { return bIsPumping; }

	/**
	 * Check if chamber is empty
	 */
	UFUNCTION(BlueprintPure, Category = "PumpAction")
	bool IsChamberEmpty() const { return bChamberEmpty; }

	/**
	 * Reset chamber state (called on unequip)
	 * Sets bChamberEmpty = false
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void ResetChamberState();

	/**
	 * Set chamber empty state (SERVER ONLY)
	 * Called when magazine becomes empty after firing
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction")
	void SetChamberEmpty(bool bEmpty);

	// ============================================
	// VIRTUAL OVERRIDES
	// ============================================

	/**
	 * Check if reload is possible - extends base with pump-action check
	 * Returns false if pump is currently cycling (bIsPumping)
	 * Returns false if magazine is already full
	 */
	virtual bool CanReload_Internal() const override;

	/**
	 * Play reload montages - override to re-attach weapon first
	 * Called from Server_StartReload (server) and OnRep_IsReloading (clients)
	 */
	virtual void PlayReloadMontages() override;

	/**
	 * Called when reload montage completes
	 * Override to handle shell-by-shell reload completion
	 *
	 * @param Montage - Montage that ended
	 * @param bInterrupted - false: completed normally, true: interrupted
	 */
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;

	/**
	 * Cancel reload - override to handle weapon re-attachment
	 * Stops montages and re-attaches weapon to original socket
	 */
	virtual void StopReloadMontages() override;

	/**
	 * Complete reload - shell inserted
	 * Called when reload montage completes successfully
	 *
	 * SERVER ONLY operation:
	 * - Re-attaches weapon back to CharacterAttachSocket
	 * - Sets bIsReloading = false
	 * - Does NOT add ammo here (done in OnShellInsert for proper timing)
	 */
	virtual void OnReloadComplete() override;

	// ============================================
	// SHELL VISUAL API (Called by AnimNotify)
	// ============================================

	/**
	 * Called by AnimNotify_GrabShell during reload animation
	 * Shows magazine mesh and attaches to character's hand socket
	 * Uses weapon's MagazineComponent for FPS/TPS visibility
	 * LOCAL operation (runs on all machines)
	 *
	 * Flow: AnimNotify_GrabShell → OnGrabShell() → Magazine visible in hand
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction|Reload")
	void OnGrabShell();

	/**
	 * Called by AnimNotify_ShellInsert during reload animation
	 * SERVER: Adds 1 ammo to magazine
	 * LOCAL: Hides magazine mesh, reattaches to weapon socket
	 *
	 * Flow: AnimNotify_ShellInsert → OnShellInsert() → Magazine hidden, +1 ammo
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction|Reload")
	void OnShellInsert();

	// ============================================
	// PUMP-ACTION INTEGRATION
	// ============================================

	/**
	 * Called by AnimNotify_ReloadPumpAction during reload montage
	 * Triggers pump-action sequence to chamber first round
	 * Only called if bNeedsPumpAfterReload is true
	 * SERVER: Sets pump state and triggers montage
	 * CLIENTS: State replicates, montages play via OnRep
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction|Reload")
	void OnReloadPumpActionNotify();

	/**
	 * Called by AnimNotify_PumpActionStart during pump-action montage
	 * Starts weapon pump-action montage (WeaponPumpActionMontage)
	 * Synchronizes weapon pump animation with character hand movement
	 * LOCAL operation (runs on all machines)
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction|Reload")
	void OnPumpActionStart();

	/**
	 * Called when pump-action sequence completes after reload
	 * - Re-attaches weapon back to CharacterAttachSocket
	 * - Completes reload (state reset)
	 */
	UFUNCTION(BlueprintCallable, Category = "PumpAction|Reload")
	void OnPumpActionAfterReloadComplete();

protected:
	// ============================================
	// INTERNAL HELPERS
	// ============================================

	/**
	 * Callback for pump-action montage end
	 * Used to detect when pump-action after reload completes
	 */
	void OnPumpActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Is pump-action after reload currently in progress?
	 * True from reload montage end until pump-action completes
	 */
	bool bPumpActionPending = false;

	/**
	 * Has shell been inserted during this reload cycle?
	 * Prevents multiple ammo additions from AnimNotify firing on multiple meshes
	 * Reset to false when reload montage starts, set to true after first OnShellInsert
	 */
	bool bShellInsertedThisReload = false;

	/**
	 * Has shell been grabbed during this reload cycle?
	 * Prevents multiple grab calls from AnimNotify firing on multiple meshes
	 * Reset to false when reload montage starts, set to true after first OnGrabShell
	 */
	bool bShellGrabbedThisReload = false;

	/**
	 * Reattach weapon to socket on character
	 * Used during reload to move weapon between sockets
	 * @param bToReloadSocket - true: attach to ReloadAttachSocket, false: attach to CharacterAttachSocket
	 */
	void ReattachWeaponToSocket(bool bToReloadSocket);

	/**
	 * Reattach weapon to specific socket on character (overload)
	 * Handles both FPS (Arms) and TPS (Body) mesh attachments
	 * @param SocketName - Socket name on character mesh to attach weapon to
	 */
	void ReattachWeaponToSocket(FName SocketName);

	/**
	 * Play pump-action montages on character meshes
	 * LOCAL operation - each machine plays independently
	 */
	void PlayPumpActionMontages();

	/**
	 * Stop pump-action montages (if interrupted)
	 * LOCAL operation
	 */
	void StopPumpActionMontages();

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

	// ============================================
	// MAGAZINE/SHELL VISIBILITY HELPERS
	// ============================================

	/**
	 * Show magazine mesh and attach to character hand socket
	 * Called during OnGrabShell to display shell in hand
	 * Uses IReloadableInterface to get magazine meshes
	 */
	void ShowAndAttachMagazineToHand();

	/**
	 * Hide magazine mesh and reattach to weapon
	 * Called during OnShellInsert after shell is inserted
	 * Magazine stays hidden until next OnGrabShell
	 */
	void HideAndAttachMagazineToWeapon();

	/**
	 * Set magazine mesh visibility
	 * @param bVisible - true to show, false to hide
	 */
	void SetMagazineVisibility(bool bVisible);
};
