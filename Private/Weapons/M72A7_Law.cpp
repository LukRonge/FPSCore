// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/M72A7_Law.h"
#include "Components/DisposableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"

AM72A7_Law::AM72A7_Law()
{
	// ============================================
	// WEAPON INFO
	// ============================================
	Name = FText::FromString("M72A7 LAW");
	Description = FText::FromString("66mm disposable anti-tank rocket launcher. Single use.");

	// ============================================
	// DISABLE INHERITED COMPONENTS
	// ============================================
	// M72A7 does NOT use BallisticsComponent, FireComponent, or ReloadComponent
	BallisticsComponent = nullptr;
	FireComponent = nullptr;
	ReloadComponent = nullptr;

	// ============================================
	// DISPOSABLE COMPONENT
	// ============================================
	DisposableComponent = CreateDefaultSubobject<UDisposableComponent>(TEXT("DisposableComponent"));

	// ============================================
	// M72A7 DEFAULTS
	// ============================================
	bHasFired = false;

	// ============================================
	// AIMING DEFAULTS
	// ============================================
	AimFOV = 55.0f;
	AimLookSpeed = 0.6f;
	LeaningScale = 0.8f;
	BreathingScale = 1.2f;
}

// ============================================
// REPLICATION
// ============================================

void AM72A7_Law::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AM72A7_Law, bHasFired);
}

void AM72A7_Law::OnRep_HasFired()
{
	// Sync DisposableComponent state
	if (DisposableComponent && bHasFired)
	{
		DisposableComponent->MarkAsUsed();
	}
}

// ============================================
// USABLE INTERFACE OVERRIDES
// ============================================

void AM72A7_Law::UseStart_Implementation(const FUseContext& Ctx)
{
	// Single-shot check
	if (bHasFired)
	{
		return;
	}

	// Block firing during equip/unequip montages
	if (bIsEquipping || bIsUnequipping)
	{
		return;
	}

	// Request server to fire
	Server_Fire();
}

void AM72A7_Law::UseStop_Implementation(const FUseContext& Ctx)
{
	// No-op for single-shot weapon
}

bool AM72A7_Law::IsUsing_Implementation() const
{
	return false;
}

// ============================================
// AMMO CONSUMER INTERFACE OVERRIDES
// ============================================

int32 AM72A7_Law::GetClip_Implementation() const
{
	return bHasFired ? 0 : 1;
}

int32 AM72A7_Law::GetClipSize_Implementation() const
{
	return 1;
}

int32 AM72A7_Law::ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx)
{
	if (!HasAuthority()) return 0;
	if (bHasFired) return 0;
	if (Requested <= 0) return 0;

	bHasFired = true;

	// Mark disposable component as used
	if (DisposableComponent)
	{
		DisposableComponent->MarkAsUsed();
	}

	return 1;
}

// ============================================
// HOLDABLE INTERFACE OVERRIDES
// ============================================

bool AM72A7_Law::CanBeUnequipped_Implementation() const
{
	// Delegate to DisposableComponent
	if (DisposableComponent && !DisposableComponent->CanBeUnequipped())
	{
		return false;
	}

	// Also check base class (montage playing, etc.)
	return Super::CanBeUnequipped_Implementation();
}

// ============================================
// PICKUPABLE INTERFACE OVERRIDES
// ============================================

bool AM72A7_Law::CanBePicked_Implementation(const FInteractionContext& Ctx) const
{
	// Delegate to DisposableComponent - used disposable items cannot be picked up
	if (DisposableComponent && !DisposableComponent->CanBePickedUp())
	{
		return false;
	}

	// Also check base class (owner check, etc.)
	return Super::CanBePicked_Implementation(Ctx);
}

// ============================================
// SERVER RPC
// ============================================

void AM72A7_Law::Server_Fire_Implementation()
{
	// Validate state
	if (bHasFired)
	{
		return;
	}

	// Block firing during equip/unequip montages (SERVER VALIDATION)
	if (bIsEquipping || bIsUnequipping)
	{
		return;
	}

	// Validate projectile class
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("M72A7_Law::Server_Fire - ProjectileClass not set!"));
		return;
	}

	// Mark as fired (SERVER AUTHORITATIVE)
	bHasFired = true;

	// Mark disposable component as used
	if (DisposableComponent)
	{
		DisposableComponent->MarkAsUsed();
	}

	// Spawn projectile (SERVER ONLY)
	SpawnProjectile();

	// Play effects on all clients (calls our overridden Multicast_PlayShootEffects_Implementation)
	Multicast_PlayShootEffects();
}

// ============================================
// BASEWEAPON OVERRIDES
// ============================================

void AM72A7_Law::Multicast_PlayShootEffects_Implementation()
{
	// M72A7 uses custom socket for muzzle flash (ProjectileSpawnSocket instead of "barrel")
	// We need to spawn muzzle VFX BEFORE calling Super, because Super uses "barrel" socket
	// Then call Super for character animations only

	const bool bIsDedicatedServer = (GetNetMode() == NM_DedicatedServer);

	// Skip muzzle VFX on dedicated server
	if (!bIsDedicatedServer && MuzzleFlashNiagara)
	{
		AActor* WeaponOwner = GetOwner();
		APawn* OwnerPawn = WeaponOwner ? Cast<APawn>(WeaponOwner) : nullptr;
		const bool bIsLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();

		// Use FPSMesh for owner, TPSMesh for others
		USkeletalMeshComponent* TargetMesh = bIsLocallyControlled ? FPSMesh : TPSMesh;
		if (TargetMesh)
		{
			UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
				MuzzleFlashNiagara,
				TargetMesh,
				ProjectileSpawnSocket,  // M72A7-specific socket
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true,
				true,
				ENCPoolMethod::None
			);

			if (NiagaraComp)
			{
				if (bIsLocallyControlled)
				{
					NiagaraComp->SetOnlyOwnerSee(true);
					NiagaraComp->SetOwnerNoSee(false);
				}
				else
				{
					NiagaraComp->SetOnlyOwnerSee(false);
					NiagaraComp->SetOwnerNoSee(true);
				}
			}
		}

		// Temporarily clear MuzzleFlashNiagara so Super doesn't spawn another one
		UNiagaraSystem* OriginalMuzzleFlash = MuzzleFlashNiagara;
		MuzzleFlashNiagara = nullptr;

		// Call base implementation for character animations
		Super::Multicast_PlayShootEffects_Implementation();

		// Restore MuzzleFlashNiagara
		MuzzleFlashNiagara = OriginalMuzzleFlash;
	}
	else
	{
		// Dedicated server or no muzzle flash - just call Super for animations
		Super::Multicast_PlayShootEffects_Implementation();
	}
}

// ============================================
// HELPERS
// ============================================

AActor* AM72A7_Law::SpawnProjectile()
{
	if (!HasAuthority() || !ProjectileClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Get spawn transform from barrel socket (prefer TPSMesh for server accuracy)
	FTransform SpawnTransform;
	USkeletalMeshComponent* SpawnMesh = TPSMesh;

	if (SpawnMesh && SpawnMesh->DoesSocketExist(ProjectileSpawnSocket))
	{
		SpawnTransform = SpawnMesh->GetSocketTransform(ProjectileSpawnSocket, ERelativeTransformSpace::RTS_World);
	}
	else
	{
		SpawnTransform.SetLocation(GetActorLocation());
		SpawnTransform.SetRotation(GetActorRotation().Quaternion());
		UE_LOG(LogTemp, Warning, TEXT("M72A7_Law::SpawnProjectile - Socket '%s' not found"), *ProjectileSpawnSocket.ToString());
	}

	// Get view direction from owner for projectile direction
	AActor* WeaponOwner = GetOwner();
	if (WeaponOwner && WeaponOwner->Implements<UViewPointProviderInterface>())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		IViewPointProviderInterface::Execute_GetShootingViewPoint(WeaponOwner, ViewLocation, ViewRotation);
		SpawnTransform.SetRotation(ViewRotation.Quaternion());
	}

	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn the projectile
	return World->SpawnActor<AActor>(ProjectileClass, SpawnTransform, SpawnParams);
}
