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

	// Apply visibility settings when owner changes
	// This ensures OnlyOwnerSee/OwnerNoSee flags work correctly for FPS/TPS visibility
	// Also handles SetVisibility(false) for FPS magazine when owner is nullptr
	//
	// IMPORTANT: Only apply if FirstPersonPrimitiveType is already set
	// This prevents race condition where SetOwner() is called before
	// MagazineChildActorComponent::InitializeChildActorVisibility() sets the type
	if (FirstPersonPrimitiveType != EFirstPersonPrimitiveType::None)
	{
		ApplyVisibilityToMeshes();
	}
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
	// Debug logging
	UE_LOG(LogTemp, Warning, TEXT("BaseMagazine::ApplyVisibilityToMeshes() - %s, FirstPersonPrimitiveType=%d, Owner=%s"),
		*GetName(),
		static_cast<int32>(FirstPersonPrimitiveType),
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));

	if (FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> EARLY RETURN: FirstPersonPrimitiveType is None!"));
		return;
	}

	// Determine visibility based on FirstPersonPrimitiveType and Owner state
	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;
	bool bVisible = true;

	// Check if we have a valid owner (weapon is equipped by someone)
	const bool bHasOwner = (GetOwner() != nullptr);

	switch (FirstPersonPrimitiveType)
	{
		case EFirstPersonPrimitiveType::FirstPerson:
			// FPS magazine - visible ONLY to owner
			// If no owner exists, this mesh should be HIDDEN (no one can see it)
			bOnlyOwnerSee = true;
			bOwnerNoSee = false;
			bVisible = bHasOwner;  // Hidden when no owner
			break;

		case EFirstPersonPrimitiveType::WorldSpaceRepresentation:
			// TPS magazine - visible to everyone EXCEPT owner
			// If no owner exists, this mesh should be VISIBLE (everyone can see it)
			bOnlyOwnerSee = false;
			bOwnerNoSee = true;
			bVisible = true;  // Always visible (OwnerNoSee handles owner case)
			break;

		default:
			break;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	UE_LOG(LogTemp, Warning, TEXT("  -> bHasOwner=%d, bVisible=%d, bOnlyOwnerSee=%d, bOwnerNoSee=%d, NumPrimitives=%d"),
		bHasOwner, bVisible, bOnlyOwnerSee, bOwnerNoSee, PrimitiveComponents.Num());

	for (UPrimitiveComponent* PrimitiveComp : PrimitiveComponents)
	{
		PrimitiveComp->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);
		PrimitiveComp->SetOnlyOwnerSee(bOnlyOwnerSee);
		PrimitiveComp->SetOwnerNoSee(bOwnerNoSee);
		PrimitiveComp->SetVisibility(bVisible);
	}
}
