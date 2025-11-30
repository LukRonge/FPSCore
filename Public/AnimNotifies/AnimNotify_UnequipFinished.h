// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_UnequipFinished.generated.h"

/**
 * UAnimNotify_UnequipFinished
 *
 * Animation notify that signals unequip animation has completed.
 * Item should now be holstered (attached to spine_03, hidden).
 *
 * Timeline Position: End of unequip animation
 *
 * Action:
 * - Navigates from AnimInstance → Character
 * - Calls character's OnUnequipMontageFinished()
 * - Triggers pending equip if item switch is in progress
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → OnUnequipMontageFinished() (handles holstering + pending equip)
 *
 * Usage in Animation:
 * - Add this notify to unequip animation montage
 * - Place at final frame of holstering animation
 * - Triggers next equip if switching weapons
 *
 * Example: M4A1 Unequip (weapon switch)
 * - Frame 0: Character starts lowering weapon
 * - Frame 20: UnequipFinished notify
 * - Weapon moves to spine_03, hides
 * - If PendingEquipItem exists, starts equip montage for new weapon
 */
UCLASS()
class FPSCORE_API UAnimNotify_UnequipFinished : public UAnimNotify
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
	 * @return Display name "UnequipFinished"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("UnequipFinished");
	}
};
