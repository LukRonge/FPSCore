// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifies/AnimNotify_MagazineIn.h"
#include "FPSCharacter.h"
#include "BaseWeapon.h"
#include "Components/ReloadComponent.h"
#include "Interfaces/ReloadableInterface.h"

void UAnimNotify_MagazineIn::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Navigate: MeshComp → Owner (Character)
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineIn: MeshComp has no owner"));
		return;
	}

	// Get character (assuming FPSCharacter)
	AFPSCharacter* Character = Cast<AFPSCharacter>(Owner);
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineIn: Owner is not AFPSCharacter"));
		return;
	}

	// Get active item (replicated property on FPSCharacter)
	AActor* ActiveItem = Character->ActiveItem;
	if (!ActiveItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineIn: No active item in inventory"));
		return;
	}

	// Check if reloadable (interface check)
	IReloadableInterface* Reloadable = Cast<IReloadableInterface>(ActiveItem);
	if (!Reloadable)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineIn: Active item does not implement IReloadableInterface"));
		return;
	}

	// Get weapon (safe cast after interface check)
	ABaseWeapon* Weapon = Cast<ABaseWeapon>(ActiveItem);
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineIn: Active item is not ABaseWeapon"));
		return;
	}

	// Get ReloadComponent
	if (!Weapon->ReloadComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_MagazineIn: Weapon has no ReloadComponent"));
		return;
	}

	// Call OnMagazineIn (LOCAL operation)
	Weapon->ReloadComponent->OnMagazineIn();

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_MagazineIn: Successfully triggered OnMagazineIn on ReloadComponent"));
}
