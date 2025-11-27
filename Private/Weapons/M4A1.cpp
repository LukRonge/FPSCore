// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/M4A1.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/FullAutoFireComponent.h"
#include "Components/BoxMagazineReloadComponent.h"
#include "Components/BallisticsComponent.h"
#include "BaseMagazine.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

AM4A1::AM4A1()
{
	// ============================================
	// WEAPON INFO
	// ============================================
	Name = FText::FromString("M4A1");
	Description = FText::FromString("5.56x45mm NATO assault rifle with full-auto fire mode.");

	// ============================================
	// CALIBER
	// ============================================
	AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

	// ============================================
	// COMPONENTS
	// ============================================

	// Full-Auto Fire Component
	FullAutoFireComponent = CreateDefaultSubobject<UFullAutoFireComponent>(TEXT("FullAutoFireComponent"));
	FullAutoFireComponent->FireRate = 800.0f;      // 800 RPM (real M4A1 spec)
	FullAutoFireComponent->SpreadScale = 1.0f;     // Normal spread
	FullAutoFireComponent->RecoilScale = 1.0f;     // Normal recoil

	// Box Magazine Reload Component
	BoxMagazineReloadComponent = CreateDefaultSubobject<UBoxMagazineReloadComponent>(TEXT("BoxMagazineReloadComponent"));
	BoxMagazineReloadComponent->MagazineOutSocketName = FName("weapon_l");
	BoxMagazineReloadComponent->MagazineInSocketName = FName("magazine");

	// ============================================
	// AIMING DEFAULTS
	// ============================================
	AimFOV = 50.0f;
	AimLookSpeed = 0.5f;
	LeaningScale = 1.0f;
	BreathingScale = 1.0f;

	// ============================================
	// M4A1 STATE DEFAULTS
	// ============================================
	bHasFiredOnce = false;
	bBoltCarrierOpen = false;
}

// ============================================
// REPLICATION
// ============================================

void AM4A1::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AM4A1, bHasFiredOnce);
	DOREPLIFETIME(AM4A1, bBoltCarrierOpen);
}

void AM4A1::OnRep_HasFiredOnce()
{
	PropagateStateToAnimInstances();
}

void AM4A1::OnRep_BoltCarrierOpen()
{
	PropagateStateToAnimInstances();
}

void AM4A1::PropagateStateToAnimInstances()
{
	// Propagate state to FPS mesh AnimInstance
	if (FPSMesh)
	{
		if (UAnimInstance* FPSAnimInstance = FPSMesh->GetAnimInstance())
		{
			// Set properties via SetRootMotionMode workaround or direct property access
			// AnimInstance reads BlueprintReadOnly properties directly from owner actor
			// Force AnimInstance to update by marking it dirty
			FPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}

	// Propagate state to TPS mesh AnimInstance
	if (TPSMesh)
	{
		if (UAnimInstance* TPSAnimInstance = TPSMesh->GetAnimInstance())
		{
			TPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}
}

// ============================================
// BASEWEAPON OVERRIDES
// ============================================

void AM4A1::Multicast_PlayMuzzleFlash_Implementation()
{
	// Call base implementation (muzzle VFX + character shoot anims)
	Super::Multicast_PlayMuzzleFlash_Implementation();

	// M4A1-specific: Play bolt carrier shoot montage on weapon meshes
	// This runs on ALL clients (server + remote clients)
	PlayWeaponMontage(BoltCarrierShootMontage);
}

void AM4A1::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// Call base implementation (triggers Multicast_PlayMuzzleFlash for character anims + muzzle VFX)
	Super::HandleShotFired_Implementation(MuzzleLocation, Direction);

	// M4A1-specific: First shot opens ejection port cover (SERVER ONLY)
	bool bStateChanged = false;
	if (!bHasFiredOnce)
	{
		bHasFiredOnce = true;
		bStateChanged = true;
	}

	// Check if magazine is empty after this shot → bolt locks back (SERVER ONLY)
	if (CurrentMagazine && CurrentMagazine->CurrentAmmo == 0)
	{
		bBoltCarrierOpen = true;
		bStateChanged = true;
	}

	// Server: propagate state to AnimInstances immediately
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (bStateChanged && HasAuthority())
	{
		PropagateStateToAnimInstances();
	}
}

void AM4A1::OnUnequipped_Implementation()
{
	// Call base implementation (cancels reload, resets aiming)
	Super::OnUnequipped_Implementation();

	// M4A1-specific: Reset state (closes ejection port cover, resets bolt)
	bHasFiredOnce = false;
	bBoltCarrierOpen = false;

	// Server: propagate state to AnimInstances immediately
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		PropagateStateToAnimInstances();
	}
}

// ============================================
// RELOADABLE INTERFACE OVERRIDE
// ============================================

void AM4A1::OnWeaponReloadComplete_Implementation()
{
	// Call base implementation
	Super::OnWeaponReloadComplete_Implementation();

	// M4A1-specific: Reset bolt carrier state (bolt goes forward after reload)
	bBoltCarrierOpen = false;

	// Server: propagate state to AnimInstances immediately
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		PropagateStateToAnimInstances();
	}
}

// ============================================
// HELPERS
// ============================================

void AM4A1::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	// Play on FPS mesh (visible to owner)
	if (FPSMesh)
	{
		UAnimInstance* FPSAnimInstance = FPSMesh->GetAnimInstance();
		if (FPSAnimInstance)
		{
			FPSAnimInstance->Montage_Play(Montage);
		}
	}

	// Play on TPS mesh (visible to others)
	if (TPSMesh)
	{
		UAnimInstance* TPSAnimInstance = TPSMesh->GetAnimInstance();
		if (TPSAnimInstance)
		{
			TPSAnimInstance->Montage_Play(Montage);
		}
	}
}
