// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ShellEject.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/BoltActionFireComponent.h"

void UAnimNotify_ShellEject::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;
	if (!Owner->Implements<UItemCollectorInterface>()) return;

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	if (!ActiveItem) return;

	// Find BoltActionFireComponent on the active item
	UBoltActionFireComponent* BoltComp = ActiveItem->FindComponentByClass<UBoltActionFireComponent>();
	if (!BoltComp) return;

	BoltComp->OnShellEject();
}
