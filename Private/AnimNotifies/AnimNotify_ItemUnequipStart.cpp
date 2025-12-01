// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ItemUnequipStart.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"

void UAnimNotify_ItemUnequipStart::Notify(
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

	// Navigate to unequipping item via interface
	// NOTE: Use GetUnequippingItem, NOT GetActiveItem!
	// During weapon switch, ActiveItem may already be set to the NEW weapon,
	// but we need to trigger collapse animation on the OLD weapon being unequipped.
	if (!Owner->Implements<UItemCollectorInterface>()) return;

	AActor* UnequippingItem = IItemCollectorInterface::Execute_GetUnequippingItem(Owner);
	if (!UnequippingItem) return;
	if (!UnequippingItem->Implements<UHoldableInterface>()) return;

	// On Listen Server, both Authority and SimulatedProxy items exist
	// Only trigger on Authority item to avoid double-fire
	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		// Local player: use Authority item only
		if (UnequippingItem->GetLocalRole() == ROLE_SimulatedProxy)
		{
			return;
		}
	}

	// Trigger item's own unequip/collapse animation
	IHoldableInterface::Execute_OnItemUnequipAnimationStart(UnequippingItem, OwnerPawn);
}
