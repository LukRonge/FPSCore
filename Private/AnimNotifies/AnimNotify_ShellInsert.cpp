// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ShellInsert.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Components/PumpActionReloadComponent.h"

void UAnimNotify_ShellInsert::Notify(
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

	// Insert shell - SERVER adds ammo, LOCAL plays VFX
	ReloadComp->OnShellInsert();
}
