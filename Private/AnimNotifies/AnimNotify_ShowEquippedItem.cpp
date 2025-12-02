// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ShowEquippedItem.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"

void UAnimNotify_ShowEquippedItem::Notify(
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

	UE_LOG(LogTemp, Warning, TEXT("[EQUIP_FLOW] AnimNotify_ShowEquippedItem - Showing item %s, Frame=%lld"),
		*ActiveItem->GetName(), GFrameCounter);

	// Show the item using SetActorHiddenInGame
	// This is called after AnimInstance has processed the montage (frame 1-2),
	// so socket position is now correct
	ActiveItem->SetActorHiddenInGame(false);
}
