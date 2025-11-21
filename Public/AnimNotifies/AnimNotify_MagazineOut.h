// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MagazineOut.generated.h"

/**
 * UAnimNotify_MagazineOut
 *
 * Animation notify that triggers magazine detachment from weapon during reload.
 *
 * Timeline Position: Early in reload animation (e.g., Frame 30 of 120)
 *
 * Action:
 * - Called during reload animation when magazine should detach from weapon
 * - Navigates from AnimInstance → Character → ActiveItem → ReloadComponent
 * - Calls ReloadComponent->OnMagazineOut()
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (IReloadableInterface check)
 *          → ABaseWeapon
 *          → ReloadComponent
 *          → OnMagazineOut() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to reload animation montage
 * - Place at frame where character's hand grabs magazine
 * - Triggers magazine attachment change (weapon → hand)
 *
 * Example: M4A1 Reload
 * - Frame 30: MagazineOut notify
 * - Magazine detaches from weapon "magazine" socket
 * - Magazine attaches to character "weapon_r" hand socket
 */
UCLASS()
class FPSCORE_API UAnimNotify_MagazineOut : public UAnimNotify
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
	 * @return Display name "MagazineOut"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("MagazineOut");
	}
};
