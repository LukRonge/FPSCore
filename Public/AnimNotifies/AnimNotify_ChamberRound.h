// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ChamberRound.generated.h"

/**
 * UAnimNotify_ChamberRound
 *
 * Animation notify that triggers round chambering during bolt-action or pump-action cycling.
 *
 * Timeline Position: Late in action animation (when bolt/pump handle is pushed forward)
 *
 * Action:
 * - Called during bolt-action or pump-action animation when next round should be chambered
 * - Navigates from AnimInstance → Character → ActiveItem → FireComponent
 * - Calls BoltActionFireComponent->OnChamberRound() or PumpActionFireComponent->OnChamberRound()
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (check for BoltActionFireComponent or PumpActionFireComponent)
 *          → FireComponent
 *          → OnChamberRound() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to bolt-action or pump-action animation montage
 * - Place at frame where bolt/pump is pushed forward (chambering point)
 * - Triggers chambering audio/visual feedback
 * - If magazine empty, sets bChamberEmpty flag
 *
 * SUPPORTED COMPONENTS:
 * - UBoltActionFireComponent (bolt-action rifles)
 * - UPumpActionFireComponent (pump-action shotguns)
 */
UCLASS()
class FPSCORE_API UAnimNotify_ChamberRound : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * Runs on all machines (LOCAL operation)
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
		return TEXT("ChamberRound");
	}
};
