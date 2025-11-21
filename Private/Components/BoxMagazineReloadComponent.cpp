// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoxMagazineReloadComponent.h"
#include "BaseWeapon.h"
#include "FPSCharacter.h"
#include "BaseMagazine.h"
#include "Components/SkeletalMeshComponent.h"

// ============================================
// VIRTUAL OVERRIDES
// ============================================

void UBoxMagazineReloadComponent::OnMagazineOut()
{
	// LOCAL operation - runs on all machines independently
	ABaseWeapon* Weapon = GetOwnerWeapon();
	AFPSCharacter* Character = GetOwnerCharacter();

	if (!Weapon || !Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnMagazineOut - Missing weapon or character"));
		return;
	}

	// Get magazines from weapon
	AActor* FPSMag = Weapon->FPSMagazineComponent->GetChildActor();
	AActor* TPSMag = Weapon->TPSMagazineComponent->GetChildActor();

	if (!FPSMag || !TPSMag)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnMagazineOut - Missing magazine actors"));
		return;
	}

	// Attach FPS magazine to character Arms socket
	AttachMagazineToSocket(FPSMag, Character->Arms, MagazineOutSocketName);

	// Attach TPS magazine to character Body socket
	AttachMagazineToSocket(TPSMag, Character->GetMesh(), MagazineOutSocketName);

	UE_LOG(LogTemp, Log, TEXT("BoxMagazineReloadComponent: Magazine detached from weapon, attached to hand"));
}

void UBoxMagazineReloadComponent::OnMagazineIn()
{
	// LOCAL operation - runs on all machines independently
	ABaseWeapon* Weapon = GetOwnerWeapon();

	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnMagazineIn - Missing weapon"));
		return;
	}

	// Get magazines from weapon
	AActor* FPSMag = Weapon->FPSMagazineComponent->GetChildActor();
	AActor* TPSMag = Weapon->TPSMagazineComponent->GetChildActor();

	if (!FPSMag || !TPSMag)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnMagazineIn - Missing magazine actors"));
		return;
	}

	// Re-attach FPS magazine to weapon FPSMesh socket
	AttachMagazineToSocket(FPSMag, Weapon->FPSMesh, MagazineInSocketName);

	// Re-attach TPS magazine to weapon TPSMesh socket
	AttachMagazineToSocket(TPSMag, Weapon->TPSMesh, MagazineInSocketName);

	UE_LOG(LogTemp, Log, TEXT("BoxMagazineReloadComponent: Magazine re-attached to weapon"));
}

void UBoxMagazineReloadComponent::OnReloadComplete()
{
	// SERVER ONLY - Gameplay state change
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnReloadComplete called on client - should only run on server"));
		return;
	}

	ABaseWeapon* Weapon = GetOwnerWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Error, TEXT("BoxMagazineReloadComponent: OnReloadComplete - Missing weapon"));
		return;
	}

	// Get magazines
	ABaseMagazine* FPSMag = Cast<ABaseMagazine>(Weapon->FPSMagazineComponent->GetChildActor());
	ABaseMagazine* TPSMag = Cast<ABaseMagazine>(Weapon->TPSMagazineComponent->GetChildActor());

	if (!FPSMag || !TPSMag)
	{
		UE_LOG(LogTemp, Error, TEXT("BoxMagazineReloadComponent: OnReloadComplete - Missing magazine actors"));
		return;
	}

	// Calculate ammo needed to fill magazine
	int32 AmmoNeeded = FPSMag->MaxCapacity - FPSMag->CurrentAmmo;

	if (AmmoNeeded <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnReloadComplete - Magazine already full"));
		bIsReloading = false;
		return;
	}

	// TODO: Check reserve ammo (future feature)
	// int32 ReserveAvailable = GetReserveAmmo();
	// AmmoNeeded = FMath::Min(AmmoNeeded, ReserveAvailable);

	// Transfer ammo (Magazine->CurrentAmmo is REPLICATED)
	// NOTE: FPS and TPS magazines are separate actors with separate CurrentAmmo
	// Both must be updated to keep visual sync across all clients
	int32 AddedFPS = FPSMag->AddAmmo(AmmoNeeded);
	int32 AddedTPS = TPSMag->AddAmmo(AmmoNeeded);

	// TODO: Subtract from reserve ammo (future feature)
	// SubtractReserveAmmo(AddedFPS);

	// Clear reload state (replicates to clients)
	bIsReloading = false;

	UE_LOG(LogTemp, Log, TEXT("BoxMagazineReloadComponent: Reload complete - Added %d rounds to magazine"), AddedFPS);
}

// ============================================
// HELPER FUNCTIONS
// ============================================

void UBoxMagazineReloadComponent::AttachMagazineToSocket(AActor* Magazine, USceneComponent* Parent, FName SocketName)
{
	if (!Magazine || !Parent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: AttachMagazineToSocket - Invalid parameters"));
		return;
	}

	USceneComponent* MagRoot = Magazine->GetRootComponent();
	if (!MagRoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: AttachMagazineToSocket - Magazine has no root component"));
		return;
	}

	// Attach to parent socket
	MagRoot->AttachToComponent(
		Parent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);

	// Reset relative transform to Identity (aligns with socket transform)
	MagRoot->SetRelativeTransform(FTransform::Identity);

	UE_LOG(LogTemp, Verbose, TEXT("BoxMagazineReloadComponent: Attached magazine to socket '%s'"), *SocketName.ToString());
}
