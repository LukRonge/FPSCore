// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/HKVP9.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/SemiAutoFireComponent.h"
#include "Components/BoxMagazineReloadComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

AHKVP9::AHKVP9()
{
	// ============================================
	// WEAPON INFO
	// ============================================
	Name = FText::FromString("HK VP9");
	Description = FText::FromString("9x19mm Parabellum striker-fired pistol with semi-auto fire mode.");

	// ============================================
	// CALIBER
	// ============================================
	AcceptedCaliberType = EAmmoCaliberType::Parabellum_9x19mm;

	// ============================================
	// COMPONENTS
	// ============================================

	// Semi-Auto Fire Component
	SemiAutoFireComponent = CreateDefaultSubobject<USemiAutoFireComponent>(TEXT("SemiAutoFireComponent"));
	SemiAutoFireComponent->FireRate = 450.0f;      // 450 RPM max (semi-auto limited by trigger speed)
	SemiAutoFireComponent->SpreadScale = 0.8f;     // Slightly more accurate than rifles
	SemiAutoFireComponent->RecoilScale = 0.7f;     // Lower recoil than rifles

	// Assign to BaseWeapon's FireComponent pointer for interface compatibility
	FireComponent = SemiAutoFireComponent;

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
	AimFOV = 60.0f;          // Wider FOV than rifles (pistol sights)
	AimLookSpeed = 0.7f;     // Faster look speed than rifles
	LeaningScale = 0.8f;     // Slightly less lean (lighter weapon)
	BreathingScale = 0.6f;   // Less sway (lighter weapon)

	// ============================================
	// VP9 STATE DEFAULTS
	// ============================================
	bSlideLockedBack = false;
}

// ============================================
// REPLICATION
// ============================================

void AHKVP9::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHKVP9, bSlideLockedBack);
}

void AHKVP9::OnRep_SlideLockedBack()
{
	PropagateStateToAnimInstances();
}

void AHKVP9::PropagateStateToAnimInstances()
{
	// Use IHoldableInterface to access meshes (Golden Rule compliance)
	if (!Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(const_cast<AHKVP9*>(this));
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(const_cast<AHKVP9*>(this));

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

void AHKVP9::Multicast_PlayShootEffects_Implementation()
{
	// Call base implementation (muzzle VFX + character shoot anims)
	Super::Multicast_PlayShootEffects_Implementation();

	// VP9-specific: Play slide shoot montage on weapon meshes
	// This runs on ALL clients (server + remote clients)
	PlayWeaponMontage(SlideShootMontage);
}

void AHKVP9::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	// Use IHoldableInterface to access meshes (Golden Rule compliance)
	if (!Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(const_cast<AHKVP9*>(this));
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(const_cast<AHKVP9*>(this));

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

void AHKVP9::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// Call base implementation (triggers Multicast_PlayShootEffects for character anims + muzzle VFX)
	Super::HandleShotFired_Implementation(MuzzleLocation, Direction);

	// SERVER ONLY: Check if magazine is empty after this shot → slide locks back
	if (HasAuthority())
	{
		// Use IAmmoConsumerInterface to check ammo (Golden Rule compliance)
		if (Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(const_cast<AHKVP9*>(this));
			if (CurrentAmmo == 0)
			{
				bSlideLockedBack = true;
				// bSlideLockedBack is REPLICATED, OnRep will update clients
				// Server must also update locally since OnRep doesn't run on server
				PropagateStateToAnimInstances();
			}
		}
	}
}

void AHKVP9::OnUnequipped_Implementation()
{
	// Call base implementation (cancels reload, resets aiming)
	Super::OnUnequipped_Implementation();

	// NOTE: bSlideLockedBack state is intentionally preserved across unequip/drop
	// The slide should remain locked back if magazine is empty
	// State is only reset in OnWeaponReloadComplete when a new magazine is inserted
}

// ============================================
// RELOADABLE INTERFACE OVERRIDE
// ============================================

void AHKVP9::OnWeaponReloadComplete_Implementation()
{
	// Call base implementation
	Super::OnWeaponReloadComplete_Implementation();

	// SERVER ONLY: Reset slide state (slide goes forward after reload)
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		bSlideLockedBack = false;
		PropagateStateToAnimInstances();
	}
}

