// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ItemEquipStart.generated.h"

/**
 * UAnimNotify_ItemEquipStart
 *
 * Animation notify that triggers item's own equip animation during character equip montage.
 * Used for items that need synchronized animations (e.g., M72A7 LAW expanding).
 *
 * Timeline Position: When item should start its own animation
 *
 * Action:
 * - Called during character equip montage at specific frame
 * - Navigates: MeshComp → Character → ActiveItem → OnItemEquipAnimationStart()
 * - Item plays its own montage (e.g., expand, unfold, extend)
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → IItemCollectorInterface::GetActiveItem()
 *          → IHoldableInterface::OnItemEquipAnimationStart() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to character equip animation montage
 * - Place at frame where item's animation should start
 * - Item's OnItemEquipAnimationStart() plays its montage via PlayWeaponMontage()
 *
 * Example: M72A7 LAW Equip
 * - Frame 0:   Character grabs launcher
 * - Frame 10:  ItemEquipStart notify → Launcher starts expanding
 * - Frame 40:  EquipReady notify → Launcher fully extended, ready to fire
 *
 * IMPORTANT: Place BEFORE AnimNotify_EquipReady in the montage timeline
 */
UCLASS()
class FPSCORE_API UAnimNotify_ItemEquipStart : public UAnimNotify
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
	 * @return Display name "ItemEquipStart"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("ItemEquipStart");
	}
};
