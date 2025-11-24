// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseSight.h"

ABaseSight::ABaseSight()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ABaseSight::BeginPlay()
{
	Super::BeginPlay();
}

FVector ABaseSight::GetAimingPoint_Implementation() const
{
	return AimingPoint;
}

AActor* ABaseSight::GetSightActor_Implementation() const
{
	return const_cast<ABaseSight*>(this);
}

TSubclassOf<UUserWidget> ABaseSight::GetAimingCrosshair_Implementation() const
{
	return AimCrossHair;
}

bool ABaseSight::ShouldHideFPSMeshWhenAiming_Implementation() const
{
	return bHideFPSMeshWhenAiming;
}

float ABaseSight::GetAimingFOV_Implementation() const
{
	return AimFOV;
}

float ABaseSight::GetAimLookSpeed_Implementation() const
{
	return AimLookSpeed;
}

float ABaseSight::GetAimLeaningScale_Implementation() const
{
	return AimLeaningScale;
}

float ABaseSight::GetAimBreathingScale_Implementation() const
{
	return AimBreathingScale;
}

void ABaseSight::SetOwner(AActor* NewOwner)
{
	Super::SetOwner(NewOwner);

	// Apply visibility settings to all mesh components when owner changes
	ApplyVisibilityToMeshes();
}

void ABaseSight::ApplyVisibilityToMeshes()
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

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComp : PrimitiveComponents)
	{
		PrimitiveComp->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);
		PrimitiveComp->SetOnlyOwnerSee(bOnlyOwnerSee);
		PrimitiveComp->SetOwnerNoSee(bOwnerNoSee);
	}
}
