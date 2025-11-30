// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EquipReady.generated.h"

/**
 * UAnimNotify_EquipReady
 *
 * Animation notify that signals equip animation has completed.
 * Item is now ready to use (can shoot, aim, reload, etc.)
 *
 * Timeline Position: End of equip animation or when weapon is "ready"
 *
 * Action:
 * - Navigates from AnimInstance → Character → ActiveItem
 * - Calls IHoldableInterface::OnEquipMontageComplete()
 * - Sets bIsEquipping = false on the item
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → ActiveItem (IHoldableInterface check)
 *          → OnEquipMontageComplete() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to equip animation montage
 * - Place at frame where weapon is fully drawn and ready
 * - Blocks shooting/aiming until this notify fires
 *
 * Example: M4A1 Equip
 * - Frame 0: Character grabs weapon from holster
 * - Frame 25: EquipReady notify - weapon is raised and ready to fire
 */
UCLASS()
class FPSCORE_API UAnimNotify_EquipReady : public UAnimNotify
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
	 * @return Display name "EquipReady"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("EquipReady");
	}
};
