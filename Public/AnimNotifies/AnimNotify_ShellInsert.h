// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ShellInsert.generated.h"

/**
 * UAnimNotify_ShellInsert
 *
 * Animation notify that triggers shell insertion during shotgun reload.
 * Called when character inserts a shell into the tube magazine.
 *
 * Timeline Position: When shell physically enters the tube magazine
 *
 * Action:
 * - Called during reload animation when shell is inserted
 * - SERVER: Adds 1 ammo to magazine via PumpActionReloadComponent
 * - LOCAL: Triggers shell insertion VFX/sound
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (check for PumpActionReloadComponent)
 *          → PumpActionReloadComponent
 *          → OnShellInsert() (SERVER + LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to shotgun reload animation montage
 * - Place at frame where shell visually enters the tube
 * - Ammo is added at this exact frame
 *
 * Example Timeline (per-shell reload):
 * Frame 0:   Character reaches for shell
 * Frame 20:  Character picks up shell
 * Frame 40:  ShellInsert notify → +1 ammo, insert VFX
 * Frame 60:  Animation ends, ready for next shell or fire
 */
UCLASS()
class FPSCORE_API UAnimNotify_ShellInsert : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * Runs on all machines (SERVER adds ammo, LOCAL plays VFX)
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
		return TEXT("ShellInsert");
	}
};
