// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_ThrowRelease.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/ThrowableInterface.h"
#include "Core/FPSGameplayTags.h"

void UAnimNotify_ThrowRelease::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_ThrowRelease::Notify - MeshComp=%s, Animation=%s"),
		MeshComp ? *MeshComp->GetName() : TEXT("NULL"),
		Animation ? *Animation->GetName() : TEXT("NULL"));

	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_ThrowRelease::Notify - No MeshComp"));
		return;
	}

	// Navigate: MeshComp -> Character -> ActiveItem -> IThrowableInterface
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_ThrowRelease::Notify - No Owner on MeshComp %s"), *MeshComp->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_ThrowRelease::Notify - Owner=%s"), *Owner->GetName());

	// Get active item via IItemCollectorInterface (no direct cast to character)
	if (!Owner->Implements<UItemCollectorInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_ThrowRelease::Notify - Owner %s doesn't implement ItemCollectorInterface"), *Owner->GetName());
		return;
	}

	AActor* ActiveItem = IItemCollectorInterface::Execute_GetActiveItem(Owner);
	if (!ActiveItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_ThrowRelease::Notify - No ActiveItem on Owner %s"), *Owner->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_ThrowRelease::Notify - ActiveItem=%s"), *ActiveItem->GetName());

	// Call OnThrowRelease via IThrowableInterface (no direct cast to ABaseGrenade)
	if (!ActiveItem->Implements<UThrowableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_ThrowRelease::Notify - ActiveItem %s doesn't implement ThrowableInterface"), *ActiveItem->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_ThrowRelease::Notify - Calling OnThrowRelease on %s"), *ActiveItem->GetName());

	// Execute throw release via interface
	IThrowableInterface::Execute_OnThrowRelease(ActiveItem);
}
