// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ShellEject.generated.h"

/**
 * UAnimNotify_ShellEject
 *
 * Animation notify that triggers shell casing ejection during bolt-action cycling.
 *
 * Timeline Position: Early in bolt-action animation (when bolt handle is pulled back)
 *
 * Action:
 * - Called during bolt-action animation when shell should eject
 * - Navigates from AnimInstance → Character → ActiveItem → BoltActionFireComponent
 * - Calls BoltActionFireComponent->OnShellEject()
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (check for BoltActionFireComponent)
 *          → BoltActionFireComponent
 *          → OnShellEject() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to bolt-action animation montage
 * - Place at frame where bolt is fully retracted (shell ejection point)
 * - Triggers shell casing spawn VFX
 */
UCLASS()
class FPSCORE_API UAnimNotify_ShellEject : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Notify callback - called when animation reaches this notify
	 * Runs on all machines (LOCAL operation)
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
		return TEXT("ShellEject");
	}
};
