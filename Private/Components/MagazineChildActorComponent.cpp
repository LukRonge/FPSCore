// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MagazineChildActorComponent.h"
#include "BaseMagazine.h"

UMagazineChildActorComponent::UMagazineChildActorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMagazineChildActorComponent::BeginPlay()
{
	Super::BeginPlay();

	// NOTE: Do NOT call InitializeChildActorVisibility() here!
	//
	// BeginPlay order issue:
	// 1. Component BeginPlay runs during actor spawn
	// 2. At this point, SetChildActorClass() may not have been called yet
	//    (happens in BaseWeapon::PostInitializeComponents -> InitMagazineComponents)
	// 3. Child actor doesn't exist yet, so visibility init would fail
	//
	// Visibility initialization is handled by:
	// - BaseWeapon::InitMagazineComponents() - calls InitializeChildActorVisibility() after SetChildActorClass()
	// - BaseWeapon::OnRep_CurrentMagazineClass() - for client replication
}

void UMagazineChildActorComponent::InitializeChildActorVisibility()
{
	UE_LOG(LogTemp, Warning, TEXT("MagazineChildActorComponent::InitializeChildActorVisibility() - %s, Component FirstPersonPrimitiveType=%d"),
		*GetName(),
		static_cast<int32>(FirstPersonPrimitiveType));

	AActor* MagazineActor = GetChildActor();
	if (!MagazineActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> NO CHILD ACTOR!"));
		return;
	}

	// Set FirstPersonPrimitiveType on child actor (ABaseMagazine)
	// Using direct cast - this component is specifically for ABaseMagazine
	// This is internal initialization, not external access pattern
	if (ABaseMagazine* Magazine = Cast<ABaseMagazine>(MagazineActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> Setting FirstPersonPrimitiveType on %s from %d to %d"),
			*Magazine->GetName(),
			static_cast<int32>(Magazine->FirstPersonPrimitiveType),
			static_cast<int32>(FirstPersonPrimitiveType));

		Magazine->FirstPersonPrimitiveType = FirstPersonPrimitiveType;
		Magazine->ApplyVisibilityToMeshes();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> Cast to ABaseMagazine FAILED for %s"), *MagazineActor->GetName());
	}
}

void UMagazineChildActorComponent::PropagateOwner(AActor* NewOwner)
{
	AActor* MagazineActor = GetChildActor();
	if (!MagazineActor)
	{
		return;
	}

	// Set owner on the magazine actor
	// BaseMagazine::SetOwner will call ApplyVisibilityToMeshes() if FirstPersonPrimitiveType is set
	MagazineActor->SetOwner(NewOwner);
}
