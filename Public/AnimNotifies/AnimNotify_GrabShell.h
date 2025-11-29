// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_GrabShell.generated.h"

/**
 * UAnimNotify_GrabShell
 *
 * Animation notify that spawns shotgun shell mesh in character's hand.
 * Place at the point in reload animation where character reaches for shell.
 *
 * Action:
 * - Calls PumpActionReloadComponent::OnGrabShell()
 * - Spawns shell mesh attached to ShellGrabSocketName (default: "shell_r")
 * - Creates both FPS (Arms) and TPS (Body) shell meshes with correct visibility
 *
 * Usage:
 * - Add to reload montage for pump-action shotguns (SPAS-12, Remington 870, etc.)
 * - Place at frame where hand reaches shell holder/pouch
 * - AnimNotify_ShellInsert will destroy the shell mesh when inserted
 *
 * IMPORTANT: This notify should be placed BEFORE AnimNotify_ShellInsert in the montage.
 * The sequence is: GrabShell (shell appears in hand) -> ShellInsert (shell disappears, +1 ammo)
 *
 * Example Timeline:
 * Frame 0:   Reload button pressed, reload montage starts
 * Frame 10:  GrabShell notify → Shell mesh spawns in right hand
 * Frame 25:  Hand moves shell toward weapon
 * Frame 40:  ShellInsert notify → Shell mesh destroyed, +1 ammo
 * Frame 50:  Hand returns to idle position
 */
UCLASS()
class FPSCORE_API UAnimNotify_GrabShell : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * LOCAL operation: Spawns shell mesh on all machines
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
		return TEXT("GrabShell");
	}
};
