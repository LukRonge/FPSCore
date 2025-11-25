// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseMagazine.h"
#include "Net/UnrealNetwork.h"

ABaseMagazine::ABaseMagazine()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
}

void ABaseMagazine::BeginPlay()
{
	Super::BeginPlay();

	// NOTE: Do NOT call ApplyVisibilityToMeshes() here!
	// BeginPlay runs during CreateChildActor() BEFORE weapon sets FirstPersonPrimitiveType.
	// Visibility is applied in:
	// 1. InitMagazineComponents() - after setting FirstPersonPrimitiveType
	// 2. SetOwner() - when owner changes (propagated from OnRep_Owner on remote clients)
}

void ABaseMagazine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseMagazine, CurrentAmmo);
}

void ABaseMagazine::SetOwner(AActor* NewOwner)
{
	Super::SetOwner(NewOwner);

	// Apply visibility settings when owner changes (same pattern as BaseSight)
	//ApplyVisibilityToMeshes();
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

void ABaseMagazine::ApplyVisibilityToMeshes()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseMagazine::ApplyVisibilityToMeshes] %s - Type: %d, Owner: %s"),
		*GetName(),
		(int32)FirstPersonPrimitiveType,
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));

	if (FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Early return - Type is None"));
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

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	UE_LOG(LogTemp, Warning, TEXT("  Found %d primitive components, OnlyOwnerSee: %d, OwnerNoSee: %d"),
		PrimitiveComponents.Num(), bOnlyOwnerSee, bOwnerNoSee);

	for (UPrimitiveComponent* PrimitiveComp : PrimitiveComponents)
	{
		PrimitiveComp->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);
		PrimitiveComp->SetOnlyOwnerSee(bOnlyOwnerSee);
		PrimitiveComp->SetOwnerNoSee(bOwnerNoSee);
	}
}
