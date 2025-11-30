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

	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_UnequipFinished] Notify - MeshComp: %s, Owner: %s"),
		MeshComp ? *MeshComp->GetName() : TEXT("NULL"),
		Owner ? *Owner->GetName() : TEXT("NULL"));

	if (!Owner) return;

	// CRITICAL FIX: Only process notify from BodyMesh to avoid firing 3 times
	// (montage plays on Body, Arms, Legs - each has own AnimInstance that fires notify)
	if (Owner->Implements<UCharacterMeshProviderInterface>())
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(Owner);
		if (MeshComp != BodyMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_UnequipFinished] SKIPPING - not BodyMesh (was: %s)"), *MeshComp->GetName());
			return;
		}
	}

	// Call character's unequip finished handler via interface (Golden Rule compliance)
	// This handles holstering the item and starting pending equip if switching
	if (!Owner->Implements<UItemCollectorInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_UnequipFinished] Owner does not implement UItemCollectorInterface!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_UnequipFinished] Calling OnUnequipMontageFinished on Owner"));
	IItemCollectorInterface::Execute_OnUnequipMontageFinished(Owner);
}
