// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseMagazine.h"
#include "Net/UnrealNetwork.h"

ABaseMagazine::ABaseMagazine()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	//SetReplicateMovement(true);
}

void ABaseMagazine::BeginPlay()
{
	Super::BeginPlay();

	// ============================================
	// INITIAL SETUP
	// ============================================
	// Call InitFPSType() to set initial FirstPersonPrimitiveType and visibility flags
	//
	// IMPORTANT: Owner chain may not be correct yet:
	// - If weapon is spawned in world (pickup): Weapon->GetOwner() = nullptr
	// - If weapon is pre-equipped: Weapon->GetOwner() = Character ✅
	//
	// InitFPSType() will be called AGAIN from FPSCharacter::SetupActiveItemLocal()
	// when weapon is equipped (runs on ALL machines via OnRep pattern)
	// This ensures visibility is correct regardless of spawn scenario

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

void ABaseMagazine::SetupOwnerAndVisibility(APawn* NewOwner, EFirstPersonPrimitiveType Type)
{
	SetOwner(NewOwner);
}

void ABaseMagazine::InitFPSType()
{
	if (FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None)
	{
		return;
	}

	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;

	switch (FirstPersonPrimitiveType)
	{
		case EFirstPersonPrimitiveType::FirstPerson:
			bOnlyOwnerSee = true;
			bOwnerNoSee = false;
			break;

		case EFirstPersonPrimitiveType::WorldSpaceRepresentation:
			bOnlyOwnerSee = false;
			bOwnerNoSee = true;
			break;

		default:
			break;
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);

	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshComponents)
	{
		if (SkeletalMesh)
		{
			SkeletalMesh->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);
			SkeletalMesh->SetOnlyOwnerSee(bOnlyOwnerSee);
			SkeletalMesh->SetOwnerNoSee(bOwnerNoSee);
		}
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	for (UStaticMeshComponent* StaticMesh : StaticMeshComponents)
	{
		if (StaticMesh)
		{
			StaticMesh->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);
			StaticMesh->SetOnlyOwnerSee(bOnlyOwnerSee);
			StaticMesh->SetOwnerNoSee(bOwnerNoSee);
		}
	}
}
