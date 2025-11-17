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

void ABaseSight::SetOwner(AActor* NewOwner)
{
	Super::SetOwner(NewOwner);

	// Apply visibility settings to all mesh components when owner changes
	ApplyVisibilityToMeshes();
}

void ABaseSight::ApplyVisibilityToMeshes()
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

	// Get all primitive components (includes StaticMesh, SkeletalMesh, and all other primitives)
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComp : PrimitiveComponents)
	{
		// Set modern UE5 API
		PrimitiveComp->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);

		// Set classic visibility flags
		PrimitiveComp->SetOnlyOwnerSee(bOnlyOwnerSee);
		PrimitiveComp->SetOwnerNoSee(bOwnerNoSee);

		UE_LOG(LogTemp, Log, TEXT("BaseSight::ApplyVisibilityToMeshes() - %s | Type: %s | OnlyOwnerSee: %s | OwnerNoSee: %s"),
			*PrimitiveComp->GetName(),
			*UEnum::GetValueAsString(FirstPersonPrimitiveType),
			bOnlyOwnerSee ? TEXT("true") : TEXT("false"),
			bOwnerNoSee ? TEXT("true") : TEXT("false"));
	}
}
