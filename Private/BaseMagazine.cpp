// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseMagazine.h"
#include "Net/UnrealNetwork.h"

ABaseMagazine::ABaseMagazine()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
}

void ABaseMagazine::BeginPlay()
{
	Super::BeginPlay();

	// Initialize FPS type visibility settings
	InitFPSType();
}

void ABaseMagazine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseMagazine, CurrentAmmo);
}

// ============================================
// SIMPLE API
// ============================================

int32 ABaseMagazine::AddAmmo(int32 Amount)
{
	const int32 RemainingSpace = MaxCapacity - CurrentAmmo;
	const int32 AmountToAdd = FMath::Min(Amount, RemainingSpace);
	CurrentAmmo += AmountToAdd;
	return AmountToAdd;
}

void ABaseMagazine::RemoveAmmo()
{
	CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
}

void ABaseMagazine::InitFPSType()
{
	// If type is None, skip initialization
	if (FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None)
	{
		return;
	}

	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;

	// Determine visibility flags based on FPS type
	switch (FirstPersonPrimitiveType)
	{
		case EFirstPersonPrimitiveType::FirstPerson:
			// Only owner sees this (first-person view)
			bOnlyOwnerSee = true;
			bOwnerNoSee = false;
			break;

		case EFirstPersonPrimitiveType::WorldSpaceRepresentation:
			// Everyone except owner sees this (third-person/world view)
			bOnlyOwnerSee = false;
			bOwnerNoSee = true;
			break;

		default:
			break;
	}

	// Get all skeletal mesh components
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);

	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshComponents)
	{
		if (SkeletalMesh)
		{
			SkeletalMesh->SetOnlyOwnerSee(bOnlyOwnerSee);
			SkeletalMesh->SetOwnerNoSee(bOwnerNoSee);
		}
	}

	// Get all static mesh components
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	for (UStaticMeshComponent* StaticMesh : StaticMeshComponents)
	{
		if (StaticMesh)
		{
			StaticMesh->SetOnlyOwnerSee(bOnlyOwnerSee);
			StaticMesh->SetOwnerNoSee(bOwnerNoSee);
		}
	}
}
