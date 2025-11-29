// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ChamberRound.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/BoltActionFireComponent.h"
#include "Components/PumpActionFireComponent.h"

void UAnimNotify_ChamberRound::Notify(
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

	// Try BoltActionFireComponent first (bolt-action rifles)
	UBoltActionFireComponent* BoltComp = ActiveItem->FindComponentByClass<UBoltActionFireComponent>();
	if (BoltComp)
	{
		BoltComp->OnChamberRound();
		return;
	}

	// Try PumpActionFireComponent (pump-action shotguns)
	UPumpActionFireComponent* PumpComp = ActiveItem->FindComponentByClass<UPumpActionFireComponent>();
	if (PumpComp)
	{
		PumpComp->OnChamberRound();
		return;
	}
}
