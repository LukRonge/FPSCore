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
