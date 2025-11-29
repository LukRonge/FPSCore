// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ShootPumpAction.generated.h"

/**
 * UAnimNotify_ShootPumpAction
 *
 * Animation notify that triggers pump-action sequence after shooting.
 * Place at the end of shoot montage (after recoil) to start pump cycling.
 *
 * Action:
 * - Calls PumpActionFireComponent::OnShootMontageEnded()
 * - SERVER: Starts pump-action sequence if pending
 * - CLIENTS: State replicates via OnRep_IsPumping
 *
 * Usage:
 * - Add to shoot montage for pump-action weapons (SPAS-12, Remington 870, etc.)
 * - Place at frame where recoil animation ends
 * - Pump-action montage will start from this point
 *
 * IMPORTANT: This notify MUST be placed in the ShootMontage, not the PumpActionMontage.
 * The sequence is: ShootMontage (recoil) -> ShootPumpAction notify -> PumpActionMontage (pump cycling)
 *
 * Example Timeline:
 * Frame 0:   Fire() called, shot fired
 * Frame 1:   ShootMontage starts (recoil animation)
 * Frame 15:  Recoil peaks
 * Frame 25:  ShootPumpAction notify → PumpActionMontage starts
 * Frame X:   PumpActionMontage plays (pump handle forward/back)
 */
UCLASS()
class FPSCORE_API UAnimNotify_ShootPumpAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * SERVER: Starts pump-action if pending
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
		return TEXT("ShootPumpAction");
	}
};
