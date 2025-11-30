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

	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EquipReady] Notify - MeshComp: %s, Owner: %s"),
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
			UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EquipReady] SKIPPING - not BodyMesh (was: %s)"), *MeshComp->GetName());
			return;
		}
	}

	if (!Owner->Implements<UItemCollectorInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EquipReady] Owner does not implement UItemCollectorInterface!"));
		return;
	}

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EquipReady] ActiveItem: %s"),
		ActiveItem ? *ActiveItem->GetName() : TEXT("NULL"));

	if (!ActiveItem) return;
	if (!ActiveItem->Implements<UHoldableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EquipReady] ActiveItem does not implement UHoldableInterface!"));
		return;
	}

	// Signal that equip montage is complete - item is ready to use
	APawn* OwnerPawn = Cast<APawn>(Owner);
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_EquipReady] Calling OnEquipMontageComplete on ActiveItem"));
	IHoldableInterface::Execute_OnEquipMontageComplete(ActiveItem, OwnerPawn);
}
