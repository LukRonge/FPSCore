// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ItemUnequipStart.generated.h"

/**
 * UAnimNotify_ItemUnequipStart
 *
 * Animation notify that triggers item's own unequip/collapse animation during character unequip montage.
 * Used for items that need synchronized animations (e.g., M72A7 LAW collapsing).
 *
 * Timeline Position: When item should start its own collapse animation
 *
 * Action:
 * - Called during character unequip montage at specific frame
 * - Navigates: MeshComp -> Character -> ActiveItem -> OnItemUnequipAnimationStart()
 * - Item plays its own collapse montage and sets state (e.g., bIsExpanded = false)
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp -> Owner (AFPSCharacter)
 *          -> IItemCollectorInterface::GetActiveItem()
 *          -> IHoldableInterface::OnItemUnequipAnimationStart() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to character unequip animation montage
 * - Place at frame where item's collapse animation should start
 * - Item's OnItemUnequipAnimationStart() sets state and plays its montage
 *
 * Example: M72A7 LAW Unequip
 * - Frame 0:   Character starts holstering launcher
 * - Frame 5:   ItemUnequipStart notify -> Launcher starts collapsing, bIsExpanded = false
 * - Frame 30:  UnequipFinished notify -> Launcher fully collapsed, holstered
 *
 * IMPORTANT: Place BEFORE AnimNotify_UnequipFinished in the montage timeline
 */
UCLASS()
class FPSCORE_API UAnimNotify_ItemUnequipStart : public UAnimNotify
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
	 * @return Display name "ItemUnequipStart"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("ItemUnequipStart");
	}
};
