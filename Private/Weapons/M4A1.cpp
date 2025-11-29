// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/M4A1.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/FullAutoFireComponent.h"
#include "Components/BoxMagazineReloadComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
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

	// Assign to BaseWeapon's FireComponent pointer for interface compatibility
	FireComponent = FullAutoFireComponent;

	// Box Magazine Reload Component
	BoxMagazineReloadComponent = CreateDefaultSubobject<UBoxMagazineReloadComponent>(TEXT("BoxMagazineReloadComponent"));
	BoxMagazineReloadComponent->MagazineOutSocketName = FName("weapon_l");
	BoxMagazineReloadComponent->MagazineInSocketName = FName("magazine");

	// Assign to BaseWeapon's ReloadComponent pointer for interface compatibility
	ReloadComponent = BoxMagazineReloadComponent;

	// Ballistics Component (standard single-projectile)
	BallisticsComponent = CreateDefaultSubobject<UBallisticsComponent>(TEXT("BallisticsComponent"));

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
	// Use IHoldableInterface to access meshes (Golden Rule compliance)
	if (!Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(const_cast<AM4A1*>(this));
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(const_cast<AM4A1*>(this));

	// Propagate state to FPS mesh AnimInstance
	if (USkeletalMeshComponent* FPSSkeletalMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* FPSAnimInstance = FPSSkeletalMesh->GetAnimInstance())
		{
			// AnimInstance reads BlueprintReadOnly properties directly from owner actor
			// Force AnimInstance to update
			FPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}

	// Propagate state to TPS mesh AnimInstance
	if (USkeletalMeshComponent* TPSSkeletalMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* TPSAnimInstance = TPSSkeletalMesh->GetAnimInstance())
		{
			TPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}
}

// ============================================
// BASEWEAPON OVERRIDES
// ============================================

void AM4A1::Multicast_PlayShootEffects_Implementation()
{
	// Call base implementation (muzzle VFX + character shoot anims)
	Super::Multicast_PlayShootEffects_Implementation();

	// M4A1-specific: Play bolt carrier shoot montage on weapon meshes
	// This runs on ALL clients (server + remote clients)
	PlayWeaponMontage(BoltCarrierShootMontage);
}

void AM4A1::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// Call base implementation (triggers Multicast_PlayShootEffects for character anims + muzzle VFX)
	Super::HandleShotFired_Implementation(MuzzleLocation, Direction);

	// SERVER ONLY: Update M4A1-specific state
	if (HasAuthority())
	{
		bool bStateChanged = false;

		// M4A1-specific: First shot opens ejection port cover
		if (!bHasFiredOnce)
		{
			bHasFiredOnce = true;
			bStateChanged = true;
		}

		// Use IAmmoConsumerInterface to check ammo (Golden Rule compliance)
		if (Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(const_cast<AM4A1*>(this));
			if (CurrentAmmo == 0)
			{
				bBoltCarrierOpen = true;
				bStateChanged = true;
			}
		}

		// Propagate state to AnimInstances immediately on server
		// Clients receive state via OnRep which calls PropagateStateToAnimInstances
		if (bStateChanged)
		{
			PropagateStateToAnimInstances();
		}
	}
}

void AM4A1::OnUnequipped_Implementation()
{
	// Call base implementation (cancels reload, resets aiming)
	Super::OnUnequipped_Implementation();

	// SERVER ONLY: Reset M4A1 state (closes ejection port cover, resets bolt)
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		bHasFiredOnce = false;
		bBoltCarrierOpen = false;
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

	// SERVER ONLY: Reset bolt carrier state (bolt goes forward after reload)
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		bBoltCarrierOpen = false;
		PropagateStateToAnimInstances();
	}
}

// ============================================
// HELPERS
// ============================================

void AM4A1::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	// Use IHoldableInterface to access meshes (Golden Rule compliance)
	if (!Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(const_cast<AM4A1*>(this));
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(const_cast<AM4A1*>(this));

	// Play on FPS mesh (visible to owner)
	if (USkeletalMeshComponent* FPSSkeletalMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* FPSAnimInstance = FPSSkeletalMesh->GetAnimInstance())
		{
			FPSAnimInstance->Montage_Play(Montage);
		}
	}

	// Play on TPS mesh (visible to others)
	if (USkeletalMeshComponent* TPSSkeletalMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* TPSAnimInstance = TPSSkeletalMesh->GetAnimInstance())
		{
			TPSAnimInstance->Montage_Play(Montage);
		}
	}
}
