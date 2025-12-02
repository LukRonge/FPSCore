// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ShowEquippedItem.generated.h"

/**
 * UAnimNotify_ShowEquippedItem
 *
 * Animation notify that makes the equipped item visible during equip montage.
 * Solves the visual lag issue where weapon appears at wrong position for a few frames.
 *
 * Problem Solved:
 * - When equip starts, item is attached to socket but AnimInstance hasn't processed
 *   the new animation yet, so socket is at old position
 * - If item becomes visible immediately, it appears at wrong location for 1-2 frames
 * - By delaying visibility until this notify fires (frame 1-2 of montage),
 *   AnimInstance has already updated bone transforms to correct positions
 *
 * Timeline Position: Place at frame 1-2 of equip montage (after AnimInstance tick)
 *
 * Action:
 * - Called during character equip montage at specific frame
 * - Navigates: MeshComp -> Character -> ActiveItem -> SetVisibility(true)
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp -> Owner (AFPSCharacter)
 *          -> IItemCollectorInterface::GetActiveItem()
 *          -> IHoldableInterface::GetTPSMeshComponent/GetFPSMeshComponent()
 *          -> SetVisibility(true)
 *
 * Usage in Animation:
 * - Add this notify to character equip animation montage
 * - Place at frame 1-2 (after initial pose is applied)
 * - Item will appear at correct hand position
 *
 * IMPORTANT: Place AFTER frame 0 to ensure AnimInstance has processed the montage
 */
UCLASS()
class FPSCORE_API UAnimNotify_ShowEquippedItem : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * Runs on all machines (LOCAL operation)
	 *
	 * @param MeshComp - Skeletal mesh component playing the animation (Body/Arms/Legs)
	 * @param Animation - Animation sequence containing this notify
	 * @param EventReference - Reference to the notify event
	 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/**
	 * Get notify name for display in editor
	 * @return Display name "ShowEquippedItem"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("ShowEquippedItem");
	}
};
