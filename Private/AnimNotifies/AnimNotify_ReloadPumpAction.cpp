// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ReloadPumpAction.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/PumpActionReloadComponent.h"

void UAnimNotify_ReloadPumpAction::Notify(
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

	// Trigger pump-action after reload (only if chamber was empty)
	ReloadComp->OnReloadPumpActionNotify();
}
