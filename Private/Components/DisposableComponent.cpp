// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisposableComponent.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

UDisposableComponent::UDisposableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDisposableComponent::MarkAsUsed()
{
	bHasBeenUsed = true;
}

void UDisposableComponent::StartDropSequence()
{
	// Prevent re-entry
	if (bInDropSequence)
	{
		return;
	}

	// Must be used first
	if (!bHasBeenUsed)
	{
		return;
	}

	bInDropSequence = true;

	// Server triggers multicast for drop montage
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Multicast_PlayDropMontage();
	}
}

void UDisposableComponent::Multicast_PlayDropMontage_Implementation()
{
	PlayDropMontageOnCharacter();
}

void UDisposableComponent::PlayDropMontageOnCharacter()
{
	if (!DropMontage)
	{
		// No drop montage - execute drop immediately on server
		AActor* Owner = GetOwner();
		if (Owner && Owner->HasAuthority())
		{
			ExecuteDrop();
		}
		return;
	}

	// Get character (weapon owner's owner)
	AActor* WeaponOwner = GetOwner();
	if (!WeaponOwner)
	{
		return;
	}

	AActor* Character = WeaponOwner->GetOwner();
	if (!Character || !Character->Implements<UCharacterMeshProviderInterface>())
	{
		return;
	}

	APawn* CharacterPawn = Cast<APawn>(Character);
	const bool bIsLocallyControlled = CharacterPawn && CharacterPawn->IsLocallyControlled();

	// FPS Animation (Owning client only - Arms mesh)
	if (bIsLocallyControlled)
	{
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(Character);
		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(DropMontage);
			}
		}
	}

	// TPS Animation (All machines - Body/Legs meshes)
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(Character);
	if (BodyMesh)
	{
		if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(DropMontage);
		}
	}

	USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(Character);
	if (LegsMesh)
	{
		if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(DropMontage);
		}
	}
}

void UDisposableComponent::ExecuteDrop()
{
	// SERVER ONLY - drop is authoritative
	AActor* WeaponOwner = GetOwner();
	if (!WeaponOwner || !WeaponOwner->HasAuthority())
	{
		return;
	}

	AActor* Character = WeaponOwner->GetOwner();
	if (!Character)
	{
		return;
	}

	// Use IItemCollectorInterface to drop weapon
	if (Character->Implements<UItemCollectorInterface>())
	{
		IItemCollectorInterface::Execute_Drop(Character, WeaponOwner);
	}
}

bool UDisposableComponent::CanBeUnequipped() const
{
	// Block during drop sequence
	return !bInDropSequence;
}

bool UDisposableComponent::CanBePickedUp() const
{
	// Used disposable items cannot be picked up again
	return !bHasBeenUsed;
}
