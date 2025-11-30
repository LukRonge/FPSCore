// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_StartDropSequence.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/DisposableComponent.h"

void UAnimNotify_StartDropSequence::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Navigate: MeshComp → Character → ActiveItem → DisposableComponent
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;
	if (!Owner->Implements<UItemCollectorInterface>()) return;

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	if (!ActiveItem) return;

	// Find DisposableComponent on active item (capability-based)
	UDisposableComponent* DisposableComp = ActiveItem->FindComponentByClass<UDisposableComponent>();
	if (!DisposableComp) return;

	DisposableComp->StartDropSequence();
}
