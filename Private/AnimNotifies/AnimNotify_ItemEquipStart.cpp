// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ItemEquipStart.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"

void UAnimNotify_ItemEquipStart::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// CRITICAL: Only process notify from BodyMesh to avoid firing 3 times
	// (montage plays on Body, Arms, Legs - each has own AnimInstance that fires notify)
	if (Owner->Implements<UCharacterMeshProviderInterface>())
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(Owner);
		if (MeshComp != BodyMesh)
		{
			return;
		}
	}

	// Navigate to active item via interface
	if (!Owner->Implements<UItemCollectorInterface>()) return;

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	if (!ActiveItem) return;
	if (!ActiveItem->Implements<UHoldableInterface>()) return;

	// On Listen Server, both Authority and SimulatedProxy items exist
	// Only trigger on Authority item to avoid double-fire
	// SimulatedProxy items will play animation via PlayWeaponMontage which handles both meshes
	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		// Local player: use Authority item only
		if (ActiveItem->GetLocalRole() == ROLE_SimulatedProxy)
		{
			return;
		}
	}

	// Trigger item's own equip animation
	IHoldableInterface::Execute_OnItemEquipAnimationStart(ActiveItem, OwnerPawn);
}
