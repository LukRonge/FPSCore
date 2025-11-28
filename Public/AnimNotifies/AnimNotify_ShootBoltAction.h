// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ShootBoltAction.generated.h"

/**
 * UAnimNotify_ShootBoltAction
 *
 * Animation notify that triggers bolt-action sequence after shooting.
 * Place at the end of shoot montage (after recoil) to start bolt cycling.
 *
 * Action:
 * - Calls BoltActionFireComponent::OnShootMontageEnded()
 * - SERVER: Starts bolt-action sequence if pending
 * - CLIENTS: State replicates via OnRep_IsCyclingBolt
 *
 * Usage:
 * - Add to shoot montage for bolt-action weapons
 * - Place at frame where recoil animation ends
 * - Bolt-action montage will start from this point
 */
UCLASS()
class FPSCORE_API UAnimNotify_ShootBoltAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("ShootBoltAction");
	}
};
