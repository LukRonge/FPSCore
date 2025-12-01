// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_UnequipFinished.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"

void UAnimNotify_UnequipFinished::Notify(
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

	// Call character's unequip finished handler via interface (Golden Rule compliance)
	// This handles holstering the item and starting pending equip if switching
	if (!Owner->Implements<UItemCollectorInterface>()) return;

	IItemCollectorInterface::Execute_OnUnequipMontageFinished(Owner);
}
