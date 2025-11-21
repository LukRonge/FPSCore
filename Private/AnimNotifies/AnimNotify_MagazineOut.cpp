// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_MagazineOut.h"
#include "FPSCharacter.h"
#include "BaseWeapon.h"
#include "Components/ReloadComponent.h"
#include "Interfaces/ReloadableInterface.h"

void UAnimNotify_MagazineOut::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Navigate: MeshComp → Owner (Character)
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineOut: MeshComp has no owner"));
		return;
	}

	// Get character (assuming FPSCharacter)
	AFPSCharacter* Character = Cast<AFPSCharacter>(Owner);
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineOut: Owner is not AFPSCharacter"));
		return;
	}

	// Get active item (replicated property on FPSCharacter)
	AActor* ActiveItem = Character->ActiveItem;
	if (!ActiveItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineOut: No active item in inventory"));
		return;
	}

	// Check if reloadable (interface check)
	IReloadableInterface* Reloadable = Cast<IReloadableInterface>(ActiveItem);
	if (!Reloadable)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineOut: Active item does not implement IReloadableInterface"));
		return;
	}

	// Get weapon (safe cast after interface check)
	ABaseWeapon* Weapon = Cast<ABaseWeapon>(ActiveItem);
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineOut: Active item is not ABaseWeapon"));
		return;
	}

	// Get ReloadComponent
	if (!Weapon->ReloadComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineOut: Weapon has no ReloadComponent"));
		return;
	}

	// Call OnMagazineOut (LOCAL operation)
	Weapon->ReloadComponent->OnMagazineOut();

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_MagazineOut: Successfully triggered OnMagazineOut on ReloadComponent"));
}
