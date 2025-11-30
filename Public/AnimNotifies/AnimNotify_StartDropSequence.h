// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_StartDropSequence.generated.h"

/**
 * UAnimNotify_StartDropSequence
 *
 * Animation notify that triggers drop sequence on disposable weapons.
 * Place at end of ShootMontage for single-use weapons like M72A7 LAW.
 *
 * Timeline Position: End of shoot animation
 *
 * Action:
 * - Navigates from AnimInstance → Character → ActiveItem
 * - Calls AM72A7_Law::StartDropSequence() (or similar method)
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → IItemCollectorInterface::GetActiveItem()
 *          → AM72A7_Law::StartDropSequence()
 *
 * Usage in Animation:
 * - Add this notify to shoot animation montage for disposable weapons
 * - Place at final frame of shooting animation
 * - Triggers transition to drop/discard animation
 */
UCLASS()
class FPSCORE_API UAnimNotify_StartDropSequence : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("StartDropSequence");
	}
};
