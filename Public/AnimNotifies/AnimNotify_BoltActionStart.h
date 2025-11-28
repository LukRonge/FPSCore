// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BoltActionStart.generated.h"

/**
 * UAnimNotify_BoltActionStart
 *
 * Animation notify that triggers weapon bolt-action montage start.
 * Used to synchronize character hand animation with weapon bolt handle movement.
 *
 * Timeline Position: When character's hand grabs the bolt handle
 *
 * Action:
 * - Called during character bolt-action animation when hand reaches bolt handle
 * - Starts weapon bolt-action montage (WeaponBoltActionMontage)
 * - Synchronizes character hand movement with weapon bolt animation
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (check for BoltActionFireComponent)
 *          → BoltActionFireComponent
 *          → PlayWeaponBoltActionMontage() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to character bolt-action animation montage
 * - Place at frame where character's hand grabs the bolt handle
 * - Weapon montage starts at this exact frame for perfect sync
 *
 * IMPORTANT: This notify should be placed BEFORE ShellEject and ChamberRound notifies
 *
 * Example Timeline:
 * Frame 0:   Character starts reaching for bolt
 * Frame 15:  BoltActionStart notify → Weapon montage starts
 * Frame 30:  ShellEject notify → Shell casing ejected
 * Frame 50:  ChamberRound notify → Next round chambered
 * Frame 60:  Animation ends
 */
UCLASS()
class FPSCORE_API UAnimNotify_BoltActionStart : public UAnimNotify
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
		return TEXT("BoltActionStart");
	}
};
