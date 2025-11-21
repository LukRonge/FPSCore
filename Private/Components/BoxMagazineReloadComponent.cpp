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

	// Attach FPS magazine to character Arms socket (Identity transform - socket defines position)
	AttachMagazineToSocket(FPSMag, Character->Arms, MagazineOutSocketName);

	// Attach TPS magazine to character Body socket (Identity transform - socket defines position)
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

	// ✅ CORRECT: Get owner weapon via helper (still uses cast internally, but encapsulated)
	ABaseWeapon* Weapon = GetOwnerWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Error, TEXT("BoxMagazineReloadComponent: OnReloadComplete - Missing weapon"));
		return;
	}

	// ✅ CORRECT: Use CurrentMagazine (single source of truth for gameplay)
	ABaseMagazine* CurrentMag = Weapon->CurrentMagazine;
	if (!CurrentMag)
	{
		UE_LOG(LogTemp, Error, TEXT("BoxMagazineReloadComponent: OnReloadComplete - CurrentMagazine is null"));
		return;
	}

	// Calculate ammo needed to fill magazine
	int32 AmmoNeeded = CurrentMag->MaxCapacity - CurrentMag->CurrentAmmo;

	UE_LOG(LogTemp, Log, TEXT("BoxMagazineReloadComponent: OnReloadComplete - CurrentAmmo: %d/%d, AmmoNeeded: %d"),
		CurrentMag->CurrentAmmo, CurrentMag->MaxCapacity, AmmoNeeded);

	if (AmmoNeeded <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoxMagazineReloadComponent: OnReloadComplete - Magazine already full (CurrentAmmo: %d, MaxCapacity: %d)"),
			CurrentMag->CurrentAmmo, CurrentMag->MaxCapacity);
		bIsReloading = false;
		return;
	}

	// TODO: Check reserve ammo via IAmmoProviderInterface (future feature)
	// AActor* ReserveAmmoProvider = GetReserveAmmoProvider();
	// if (ReserveAmmoProvider && ReserveAmmoProvider->Implements<UAmmoProviderInterface>())
	// {
	//     int32 Available = IAmmoProviderInterface::Execute_GetAvailableAmmo(ReserveAmmoProvider);
	//     AmmoNeeded = FMath::Min(AmmoNeeded, Available);
	// }

	// ✅ CORRECT: Add ammo to CurrentMagazine (single source of truth)
	// CurrentMagazine->CurrentAmmo is REPLICATED - will sync to all clients
	int32 AddedAmmo = CurrentMag->AddAmmo(AmmoNeeded);

	// ✅ CORRECT: Synchronize visual magazines (FPS/TPS) with gameplay magazine
	// This ensures visual consistency across all clients
	Weapon->SyncVisualMagazines();

	// TODO: Subtract from reserve ammo via IAmmoProviderInterface (future feature)
	// IAmmoProviderInterface::Execute_TakeAmmo(ReserveAmmoProvider, AddedAmmo);

	// Clear reload state (replicates to clients)
	bIsReloading = false;

	UE_LOG(LogTemp, Log, TEXT("BoxMagazineReloadComponent: Reload complete - Added %d rounds (new total: %d/%d)"),
		AddedAmmo, CurrentMag->CurrentAmmo, CurrentMag->MaxCapacity);
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

	// ✅ CORRECT: ALWAYS use Identity transform
	// Socket itself defines the correct position and rotation
	// Magazine has NO relative offset - it snaps exactly to socket transform
	MagRoot->SetRelativeTransform(FTransform::Identity);

	UE_LOG(LogTemp, Verbose, TEXT("BoxMagazineReloadComponent: Attached magazine to socket '%s' (Identity transform)"),
		*SocketName.ToString());
}
