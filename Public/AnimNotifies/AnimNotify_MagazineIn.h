// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MagazineIn.generated.h"

/**
 * UAnimNotify_MagazineIn
 *
 * Animation notify that triggers magazine re-attachment to weapon during reload.
 *
 * Timeline Position: Late in reload animation (e.g., Frame 90 of 120)
 *
 * Action:
 * - Called during reload animation when magazine should re-attach to weapon
 * - Navigates from AnimInstance → Character → ActiveItem → ReloadComponent
 * - Calls ReloadComponent->OnMagazineIn()
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (IReloadableInterface check)
 *          → ABaseWeapon
 *          → ReloadComponent
 *          → OnMagazineIn() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to reload animation montage
 * - Place at frame where character inserts magazine into weapon
 * - Triggers magazine attachment change (hand → weapon)
 *
 * Example: M4A1 Reload
 * - Frame 90: MagazineIn notify
 * - Magazine detaches from character "weapon_r" hand socket
 * - Magazine attaches to weapon "magazine" socket
 */
UCLASS()
class FPSCORE_API UAnimNotify_MagazineIn : public UAnimNotify
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
	 * @return Display name "MagazineIn"
	 */
	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("MagazineIn");
	}
};
