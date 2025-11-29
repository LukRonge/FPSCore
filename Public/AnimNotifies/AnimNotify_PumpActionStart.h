// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PumpActionStart.generated.h"

/**
 * UAnimNotify_PumpActionStart
 *
 * Animation notify that triggers weapon pump-action montage start.
 * Used to synchronize character hand animation with weapon pump handle movement.
 *
 * Timeline Position: When character's hand grabs the pump handle
 *
 * Action:
 * - Called during character pump-action animation when hand reaches pump handle
 * - Starts weapon pump-action montage (WeaponPumpActionMontage)
 * - Synchronizes character hand movement with weapon pump animation
 * - LOCAL operation (runs on all machines independently)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → InventoryComponent
 *          → ActiveItem (check for PumpActionFireComponent)
 *          → PumpActionFireComponent
 *          → OnPumpActionStart() (LOCAL)
 *
 * Usage in Animation:
 * - Add this notify to character pump-action animation montage
 * - Place at frame where character's hand grabs the pump handle
 * - Weapon montage starts at this exact frame for perfect sync
 *
 * IMPORTANT: This notify should be placed BEFORE ShellEject and ChamberRound notifies
 *
 * Example Timeline:
 * Frame 0:   Character starts reaching for pump handle
 * Frame 15:  PumpActionStart notify → Weapon montage starts
 * Frame 30:  ShellEject notify → Shell casing ejected
 * Frame 50:  ChamberRound notify → Next round chambered
 * Frame 60:  Animation ends
 */
UCLASS()
class FPSCORE_API UAnimNotify_PumpActionStart : public UAnimNotify
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
		return TEXT("PumpActionStart");
	}
};
