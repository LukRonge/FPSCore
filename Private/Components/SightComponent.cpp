// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SightComponent.h"
#include "BaseSightActor.h"
#include "BaseWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

USightComponent::USightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Component does not replicate (pure configuration)
	SetIsReplicatedByDefault(false);
}

void USightComponent::BeginPlay()
{
	Super::BeginPlay();

	// Spawn modular sight actors if configured
	SpawnSightActors();
}

FVector USightComponent::CalculateAimingHandsOffset_Implementation() const
{
	// Default implementation: Negate aiming point to bring sight to eye
	// When aiming, camera moves to origin (0,0,0) relative to hands
	// We want AimingPoint to align with camera, so we offset hands by -AimingPoint
	// Example: If AimingPoint is (10, 0, 5), hands move to (-10, 0, -5)
	return -AimingPoint;
}

float USightComponent::GetEffectiveAimFOV(float BaseFOV) const
{
	// If AimFOV explicitly set, use it
	if (AimFOV > 0.0f)
	{
		return AimFOV;
	}

	// Otherwise calculate from magnification
	// Magnification 2.0 = FOV / 2
	// Magnification 4.0 = FOV / 4
	return BaseFOV / FMath::Max(Magnification, 1.0f);
}

FVector USightComponent::GetAimingPointWorldLocation() const
{
	USkeletalMeshComponent* Mesh = GetOwnerWeaponMesh();
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}

	// Transform aiming point from weapon local space to world space
	return Mesh->GetComponentTransform().TransformPosition(AimingPoint);
}

FVector USightComponent::GetRearSightWorldLocation() const
{
	USkeletalMeshComponent* Mesh = GetOwnerWeaponMesh();
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}

	// Get bone/socket transform
	FTransform BoneTransform = Mesh->GetSocketTransform(AttachmentRearBone);

	// Apply offset
	return BoneTransform.TransformPosition(RearOffset);
}

FVector USightComponent::GetFrontSightWorldLocation() const
{
	USkeletalMeshComponent* Mesh = GetOwnerWeaponMesh();
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}

	// Get bone/socket transform
	FTransform BoneTransform = Mesh->GetSocketTransform(AttachmentFrontBone);

	// Apply offset
	return BoneTransform.TransformPosition(FrontOffset);
}

void USightComponent::SpawnSightActors()
{
	// Only spawn on server (sights are visual, don't need replication)
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	ABaseWeapon* Weapon = Cast<ABaseWeapon>(GetOwner());
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("SightComponent::SpawnSightActors() - Owner is not a weapon!"));
		return;
	}

	USkeletalMeshComponent* WeaponMesh = GetOwnerWeaponMesh();
	if (!WeaponMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("SightComponent::SpawnSightActors() - No weapon mesh found!"));
		return;
	}

	// Spawn rear sight actor
	if (RearSightClass && !RearSightActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Weapon;
		SpawnParams.Instigator = Weapon->GetInstigator();

		RearSightActor = GetWorld()->SpawnActor<ABaseSightActor>(RearSightClass, FTransform::Identity, SpawnParams);

		if (RearSightActor)
		{
			// Attach to weapon mesh at rear sight bone
			RearSightActor->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachmentRearBone);
			RearSightActor->SetActorRelativeLocation(RearOffset);

			// Setup owner visibility
			if (APawn* OwnerPawn = Cast<APawn>(Weapon->GetOwner()))
			{
				RearSightActor->SetupOwnerAndVisibility(OwnerPawn);
			}

			UE_LOG(LogTemp, Log, TEXT("✓ SightComponent::SpawnSightActors() - Rear sight spawned: %s"),
				*RearSightClass->GetName());
		}
	}

	// Spawn front sight actor
	if (FrontSightClass && !FrontSightActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Weapon;
		SpawnParams.Instigator = Weapon->GetInstigator();

		FrontSightActor = GetWorld()->SpawnActor<ABaseSightActor>(FrontSightClass, FTransform::Identity, SpawnParams);

		if (FrontSightActor)
		{
			// Attach to weapon mesh at front sight bone
			FrontSightActor->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachmentFrontBone);
			FrontSightActor->SetActorRelativeLocation(FrontOffset);

			// Setup owner visibility
			if (APawn* OwnerPawn = Cast<APawn>(Weapon->GetOwner()))
			{
				FrontSightActor->SetupOwnerAndVisibility(OwnerPawn);
			}

			UE_LOG(LogTemp, Log, TEXT("✓ SightComponent::SpawnSightActors() - Front sight spawned: %s"),
				*FrontSightClass->GetName());
		}
	}
}

void USightComponent::DestroySightActors()
{
	if (RearSightActor)
	{
		RearSightActor->Destroy();
		RearSightActor = nullptr;
	}

	if (FrontSightActor)
	{
		FrontSightActor->Destroy();
		FrontSightActor = nullptr;
	}
}

void USightComponent::UpdateSightVisibility(bool bIsAiming)
{
	// Update modular sight actor visibility
	if (RearSightActor)
	{
		RearSightActor->UpdateAimingVisibility(bIsAiming);
	}

	if (FrontSightActor)
	{
		FrontSightActor->UpdateAimingVisibility(bIsAiming);
	}

	// Update weapon mesh visibility if configured
	if (bHideFPSMeshWhenAiming)
	{
		ABaseWeapon* Weapon = Cast<ABaseWeapon>(GetOwner());
		if (Weapon && Weapon->FPSMesh)
		{
			// Hide FPS mesh when aiming, show when not aiming
			Weapon->FPSMesh->SetVisibility(!bIsAiming, true);

			UE_LOG(LogTemp, Log, TEXT("SightComponent::UpdateSightVisibility() - FPS mesh visibility: %s"),
				bIsAiming ? TEXT("Hidden") : TEXT("Visible"));
		}
	}
}

USkeletalMeshComponent* USightComponent::GetOwnerWeaponMesh() const
{
	ABaseWeapon* Weapon = Cast<ABaseWeapon>(GetOwner());
	if (!Weapon)
	{
		return nullptr;
	}

	// Prefer FPS mesh (owner sees this), fallback to TPS mesh
	if (Weapon->FPSMesh)
	{
		return Weapon->FPSMesh;
	}

	return Weapon->TPSMesh;
}
