// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_EquipReady.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"

void UAnimNotify_EquipReady::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// CRITICAL FIX: Only process notify from BodyMesh to avoid firing 3 times
	// (montage plays on Body, Arms, Legs - each has own AnimInstance that fires notify)
	if (Owner->Implements<UCharacterMeshProviderInterface>())
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(Owner);
		if (MeshComp != BodyMesh)
		{
			return;
		}
	}

	if (!Owner->Implements<UItemCollectorInterface>()) return;

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	if (!ActiveItem) return;
	if (!ActiveItem->Implements<UHoldableInterface>()) return;

	// Signal that equip montage is complete - item is ready to use
	APawn* OwnerPawn = Cast<APawn>(Owner);
	IHoldableInterface::Execute_OnEquipMontageComplete(ActiveItem, OwnerPawn);
}
