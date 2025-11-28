// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ReloadBoltAction.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/BoltActionReloadComponent.h"

void UAnimNotify_ReloadBoltAction::Notify(
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

	// Find BoltActionReloadComponent on the active item
	UBoltActionReloadComponent* ReloadComp = ActiveItem->FindComponentByClass<UBoltActionReloadComponent>();
	if (!ReloadComp) return;

	// Trigger bolt-action sequence from reload
	ReloadComp->OnReloadBoltActionNotify();
}
