// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxMagazineReloadComponent.h"
#include "BoltActionReloadComponent.generated.h"

class UBoltActionFireComponent;

/**
 * UBoltActionReloadComponent
 *
 * Bolt-action rifle reload implementation.
 * Extends BoxMagazineReloadComponent with bolt-action cycling at reload completion.
 *
 * Reload Sequence:
 * 1. Weapon re-attaches to ReloadAttachSocket (e.g., weapon_l for left hand support)
 * 2. Animation starts (Body/Arms/Legs montages)
 * 3. AnimNotify "MagazineOut" fires → OnMagazineOut()
 *    - Detach magazine from weapon
 *    - Attach to character hand socket
 * 4. AnimNotify "MagazineIn" fires → OnMagazineIn()
 *    - Detach magazine from hand
 *    - Re-attach to weapon socket
 * 5. AnimNotify "ReloadBoltAction" fires → OnReloadBoltActionNotify()
 *    - Start bolt-action sequence to chamber first round
 *    - BoltActionMontage starts (may overlap with reload montage end)
 * 6. Bolt-action completes → OnBoltActionAfterReloadComplete()
 *    - Weapon re-attaches back to CharacterAttachSocket
 *    - bIsReloading = false
 *    - SERVER: Transfer ammo to magazine
 *
 * Key Differences from BoxMagazineReloadComponent:
 * - Weapon re-attaches to different socket during reload
 * - Bolt-action triggered by AnimNotify during reload (not at montage end)
 * - Chamber state must be set to ready after reload
 *
 * Used by: Bolt-action sniper rifles (Sako 85, AWM, Remington 700)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UBoltActionReloadComponent : public UBoxMagazineReloadComponent
{
	GENERATED_BODY()

public:
	UBoltActionReloadComponent();

	// ============================================
	// DESIGNER DEFAULTS - RELOAD BEHAVIOR
	// ============================================

	// Should weapon re-attach to ReloadAttachSocket during reload?
	// true = weapon moves to ReloadAttachSocket (bolt-action rifles need hand repositioning)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Reload")
	bool bReattachDuringReload = true;

	// ============================================
	// VIRTUAL OVERRIDES
	// ============================================

	/**
	 * Check if reload is possible - extends base with bolt-action check
	 * Returns false if bolt is currently cycling (bIsCyclingBolt)
	 */
	virtual bool CanReload_Internal() const override;

	/**
	 * Play reload montages - override to re-attach weapon first
	 * Called from Server_StartReload (server) and OnRep_IsReloading (clients)
	 */
	virtual void PlayReloadMontages() override;

	/**
	 * Called when reload montage completes
	 * Override to trigger bolt-action sequence instead of immediate completion
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

	// ============================================
	// BOLT-ACTION INTEGRATION
	// ============================================

	/**
	 * Called by AnimNotify_ReloadBoltAction during reload montage
	 * Triggers bolt-action sequence to chamber first round
	 * SERVER: Sets bolt state and triggers montage
	 * CLIENTS: State replicates, montages play via OnRep
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction|Reload")
	void OnReloadBoltActionNotify();

	/**
	 * Called when bolt-action sequence completes after reload
	 * - Re-attaches weapon back to CharacterAttachSocket
	 * - Completes reload (ammo transfer, state reset)
	 */
	UFUNCTION(BlueprintCallable, Category = "BoltAction|Reload")
	void OnBoltActionAfterReloadComplete();

protected:
	// ============================================
	// INTERNAL HELPERS
	// ============================================

	/**
	 * Get BoltActionFireComponent from owner weapon
	 * @return BoltActionFireComponent or nullptr
	 */
	UBoltActionFireComponent* GetBoltActionFireComponent() const;

	/**
	 * Callback for bolt-action montage end (triggered by BoltActionFireComponent)
	 * Used to detect when bolt-action after reload completes
	 */
	void OnBoltActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Is bolt-action after reload currently in progress?
	 * True from reload montage end until bolt-action completes
	 */
	bool bBoltActionPending = false;
};
