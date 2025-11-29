// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_PumpActionStart.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/PumpActionFireComponent.h"

void UAnimNotify_PumpActionStart::Notify(
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

	// Find PumpActionFireComponent on the active item
	UPumpActionFireComponent* PumpComp = ActiveItem->FindComponentByClass<UPumpActionFireComponent>();
	if (!PumpComp) return;

	// Start weapon pump-action montage (synchronizes with character hand animation)
	PumpComp->OnPumpActionStart();
}
