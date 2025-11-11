// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseSightActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ABaseSightActor::ABaseSightActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Sights don't replicate (visual only, attached to replicated weapon)
	bReplicates = false;

	// Create root component
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create third-person mesh (visible to everyone except owner)
	TPSMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(RootComponent);
	TPSMesh->SetOnlyOwnerSee(false);
	TPSMesh->SetOwnerNoSee(true);
	TPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create first-person mesh (visible only to owner)
	FPSMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(RootComponent);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetOwnerNoSee(false);
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABaseSightActor::BeginPlay()
{
	Super::BeginPlay();

	// Initialize reticle material if sight mesh has material
	UpdateReticleMaterial();
}

void ABaseSightActor::SetupOwnerAndVisibility(APawn* NewOwner)
{
	// Set owner for proper visibility control
	SetOwner(NewOwner);

	// Apply visibility settings
	if (NewOwner)
	{
		// Weapon is equipped - setup FPS/TPS visibility
		FPSMesh->SetOnlyOwnerSee(true);
		FPSMesh->SetOwnerNoSee(false);

		TPSMesh->SetOnlyOwnerSee(false);
		TPSMesh->SetOwnerNoSee(true);

		UE_LOG(LogTemp, Log, TEXT("BaseSightActor::SetupOwnerAndVisibility() - Owner: %s"),
			*NewOwner->GetName());
	}
	else
	{
		// Weapon dropped - show both meshes to everyone
		FPSMesh->SetOnlyOwnerSee(false);
		FPSMesh->SetOwnerNoSee(false);

		TPSMesh->SetOnlyOwnerSee(false);
		TPSMesh->SetOwnerNoSee(false);

		UE_LOG(LogTemp, Log, TEXT("BaseSightActor::SetupOwnerAndVisibility() - Owner cleared"));
	}
}

void ABaseSightActor::UpdateAimingVisibility_Implementation(bool bIsAiming)
{
	// Default implementation: No visibility changes
	// Override in Blueprint for sight-specific behavior

	// Example use cases:
	// - Rear sight aperture: Hide when ADS (looking through it)
	// - Front sight post: Show when ADS
	// - Flip-up sights: Rotate when ADS

	UE_LOG(LogTemp, Verbose, TEXT("BaseSightActor::UpdateAimingVisibility() - Aiming: %s"),
		bIsAiming ? TEXT("True") : TEXT("False"));
}

void ABaseSightActor::UpdateReticleMaterial()
{
	// Create dynamic material instance if not already created
	if (!ReticleMaterial && FPSMesh && FPSMesh->GetMaterial(0))
	{
		ReticleMaterial = FPSMesh->CreateDynamicMaterialInstance(0);
	}

	// Update material parameters
	if (ReticleMaterial)
	{
		ReticleMaterial->SetScalarParameterValue(FName("Brightness"), ReticleBrightness);
		ReticleMaterial->SetVectorParameterValue(FName("Color"), ReticleColor);

		UE_LOG(LogTemp, Log, TEXT("✓ BaseSightActor::UpdateReticleMaterial() - Brightness: %.2f, Color: %s"),
			ReticleBrightness, *ReticleColor.ToString());
	}
}
