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
	// Set owner (always set, even if nullptr to clear old owner)
	// This is critical for drop scenario: SetOwner(nullptr) clears old owner
	SetOwner(NewOwner);

	// Set visibility type
	//FirstPersonPrimitiveType = Type;

	// Apply to all mesh components
	//InitFPSType();

	UE_LOG(LogTemp, Log, TEXT("✓ BaseMagazine::SetupOwnerAndVisibility() - %s | Owner: %s | Type: %s"),
		*GetName(),
		NewOwner ? *NewOwner->GetName() : TEXT("nullptr"),
		*UEnum::GetValueAsString(Type));
}

void ABaseMagazine::InitFPSType()
{
	// If type is None, skip initialization
	if (FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None)
	{
		return;
	}

	// ============================================
	// COMBINED APPROACH: SetFirstPersonPrimitiveType() + Classic Visibility Flags
	// ============================================
	// Use both modern UE5 API and classic visibility flags for maximum compatibility
	// Owner chain: Character → Weapon → Magazine (ChildActor)

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
			// Set modern UE5 API
			SkeletalMesh->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);

			// Set classic visibility flags
			SkeletalMesh->SetOnlyOwnerSee(bOnlyOwnerSee);
			SkeletalMesh->SetOwnerNoSee(bOwnerNoSee);

			UE_LOG(LogTemp, Log, TEXT("BaseMagazine::InitFPSType() - SkeletalMesh: %s | Type: %s | OnlyOwnerSee: %s | OwnerNoSee: %s"),
				*SkeletalMesh->GetName(),
				*UEnum::GetValueAsString(FirstPersonPrimitiveType),
				bOnlyOwnerSee ? TEXT("true") : TEXT("false"),
				bOwnerNoSee ? TEXT("true") : TEXT("false"));
		}
	}

	// Get all static mesh components
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	for (UStaticMeshComponent* StaticMesh : StaticMeshComponents)
	{
		if (StaticMesh)
		{
			// Set modern UE5 API
			StaticMesh->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);

			// Set classic visibility flags
			StaticMesh->SetOnlyOwnerSee(bOnlyOwnerSee);
			StaticMesh->SetOwnerNoSee(bOwnerNoSee);

			UE_LOG(LogTemp, Log, TEXT("BaseMagazine::InitFPSType() - StaticMesh: %s | Type: %s | OnlyOwnerSee: %s | OwnerNoSee: %s"),
				*StaticMesh->GetName(),
				*UEnum::GetValueAsString(FirstPersonPrimitiveType),
				bOnlyOwnerSee ? TEXT("true") : TEXT("false"),
				bOwnerNoSee ? TEXT("true") : TEXT("false"));
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT("✓ BaseMagazine::InitFPSType() - %s | Type: %s | OnlyOwnerSee: %s | OwnerNoSee: %s"),
				*GetName(),
				*UEnum::GetValueAsString(FirstPersonPrimitiveType),
				bOnlyOwnerSee ? TEXT("true") : TEXT("false"),
				bOwnerNoSee ? TEXT("true") : TEXT("false")));
	}
}
