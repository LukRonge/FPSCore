// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_MagazineOut.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/ReloadableInterface.h"
#include "Components/ReloadComponent.h"

void UAnimNotify_MagazineOut::Notify(
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
	if (!ActiveItem->Implements<UReloadableInterface>()) return;

	UReloadComponent* ReloadComp = IReloadableInterface::Execute_GetReloadComponent(ActiveItem);
	if (!ReloadComp) return;

	ReloadComp->OnMagazineOut();
}
