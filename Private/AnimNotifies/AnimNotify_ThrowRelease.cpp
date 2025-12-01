// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ThrowRelease.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/ThrowableInterface.h"

void UAnimNotify_ThrowRelease::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Navigate: MeshComp -> Character -> ActiveItem -> IThrowableInterface
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Get active item via IItemCollectorInterface (no direct cast to character)
	if (!Owner->Implements<UItemCollectorInterface>())
	{
		return;
	}

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	if (!ActiveItem)
	{
		return;
	}

	// Call OnThrowRelease via IThrowableInterface (no direct cast to ABaseGrenade)
	if (!ActiveItem->Implements<UThrowableInterface>())
	{
		return;
	}

	// Execute throw release via interface
	IThrowableInterface::Execute_OnThrowRelease(ActiveItem);
}
