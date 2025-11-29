// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_GrabShell.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/PumpActionReloadComponent.h"

void UAnimNotify_GrabShell::Notify(
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

	// Find PumpActionReloadComponent on the active item
	UPumpActionReloadComponent* ReloadComp = ActiveItem->FindComponentByClass<UPumpActionReloadComponent>();
	if (!ReloadComp) return;

	// Spawn shell mesh in character's hand
	ReloadComp->OnGrabShell();
}
