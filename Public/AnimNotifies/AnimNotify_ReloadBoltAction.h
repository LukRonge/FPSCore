// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ReloadBoltAction.generated.h"

/**
 * UAnimNotify_ReloadBoltAction
 *
 * Animation notify that triggers bolt-action sequence during reload.
 * Used in reload montage to start chambering the first round after magazine is inserted.
 *
 * Timeline Position: After magazine is fully inserted, before reload animation ends
 *
 * Action:
 * - Triggers bolt-action sequence to chamber first round from new magazine
 * - Starts character bolt-action montage (BoltActionMontage)
 * - Weapon bolt-action montage triggered later by AnimNotify_BoltActionStart
 * - SERVER: Sets bIsCyclingBolt = true, bChamberEmpty = false
 * - CLIENTS: Play montages via OnRep_IsCyclingBolt
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → IItemCollectorInterface::GetActiveItem()
 *          → ActiveItem (ABaseWeapon with BoltActionReloadComponent)
 *          → BoltActionReloadComponent::OnReloadBoltActionNotify()
 *
 * Usage in Animation:
 * - Add this notify to reload animation montage
 * - Place AFTER AnimNotify_MagazineIn (magazine fully inserted)
 * - Place at frame where character should start bolt-action motion
 *
 * IMPORTANT: This is different from AnimNotify_BoltActionStart!
 * - AnimNotify_BoltActionStart: Syncs weapon montage with character hand (during bolt-action)
 * - AnimNotify_ReloadBoltAction: Starts entire bolt-action sequence (during reload)
 *
 * Example Reload Timeline:
 * Frame 0:   Reload starts, hand reaches for magazine
 * Frame 20:  MagazineOut notify → Old magazine detached
 * Frame 40:  MagazineIn notify → New magazine attached
 * Frame 50:  ReloadBoltAction notify → Bolt-action sequence starts ← THIS NOTIFY
 * Frame 50+: BoltActionMontage plays (overlaps or replaces remaining reload)
 *   Frame 55: BoltActionStart notify → Weapon bolt montage starts
 *   Frame 70: ShellEject notify → Shell casing ejected (if any)
 *   Frame 90: ChamberRound notify → Round chambered
 * Frame 100: BoltActionMontage ends → Reload complete
 */
UCLASS()
class FPSCORE_API UAnimNotify_ReloadBoltAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * SERVER: Triggers bolt-action state change and montage
	 * CLIENTS: State replicates via OnRep, montages play locally
	 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/**
	 * Get notify name for display in editor
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("ReloadBoltAction");
	}
};
