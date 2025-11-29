// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ReloadPumpAction.generated.h"

/**
 * UAnimNotify_ReloadPumpAction
 *
 * Animation notify that triggers pump-action sequence during reload.
 * Used when chamber was empty and first shell needs to be chambered.
 *
 * Timeline Position: After shell insertion, when pump-action should start
 *
 * Action:
 * - Called during reload animation after shell is inserted
 * - Only triggers if chamber was empty (bNeedsPumpAfterReload = true)
 * - Starts pump-action sequence to chamber the first round
 * - SERVER: Sets pump state, triggers montage
 * - CLIENTS: State replicates, montages play via OnRep
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (check for PumpActionReloadComponent)
 *          → PumpActionReloadComponent
 *          → OnReloadPumpActionNotify() (SERVER)
 *
 * Usage in Animation:
 * - Add this notify to shotgun reload animation montage
 * - Place at frame AFTER ShellInsert notify
 * - Only fires pump-action if chamber was empty when reload started
 *
 * Example Timeline (first shell when chamber empty):
 * Frame 0:   Character reaches for shell
 * Frame 20:  Character picks up shell
 * Frame 40:  ShellInsert notify → +1 ammo
 * Frame 50:  ReloadPumpAction notify → Pump-action starts (if chamber was empty)
 * Frame X:   Pump-action montage plays...
 *
 * NOTE: If chamber was NOT empty, this notify does nothing
 * (bNeedsPumpAfterReload check in OnReloadPumpActionNotify)
 */
UCLASS()
class FPSCORE_API UAnimNotify_ReloadPumpAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * SERVER: Triggers pump-action if needed
	 * CLIENTS: State replicates via OnRep
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
		return TEXT("ReloadPumpAction");
	}
};
