// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/HKVP9.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/SemiAutoFireComponent.h"
#include "Components/BoxMagazineReloadComponent.h"
#include "Components/BallisticsComponent.h"
#include "BaseMagazine.h"
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

	// Box Magazine Reload Component
	BoxMagazineReloadComponent = CreateDefaultSubobject<UBoxMagazineReloadComponent>(TEXT("BoxMagazineReloadComponent"));
	BoxMagazineReloadComponent->MagazineOutSocketName = FName("weapon_l");
	BoxMagazineReloadComponent->MagazineInSocketName = FName("magazine");

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
	// AnimBP reads bSlideLockedBack via PropertyAccess from this weapon actor
	// PropertyAccess refreshes on animation tick, but we can force update via NativeUpdateAnimation
	// This ensures AnimBP sees the new value immediately

	if (FPSMesh)
	{
		if (UAnimInstance* FPSAnimInstance = FPSMesh->GetAnimInstance())
		{
			FPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}

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

void AHKVP9::Multicast_PlayShootEffects_Implementation()
{
	// Call base implementation (muzzle VFX + character anims)
	Super::Multicast_PlayShootEffects_Implementation();

	// VP9-specific: Play slide shoot montage on appropriate weapon mesh
	// Based on IsLocallyControlled - consistent with base class architecture
	if (!SlideShootMontage)
	{
		return;
	}

	AActor* WeaponOwner = GetOwner();
	APawn* OwnerPawn = WeaponOwner ? Cast<APawn>(WeaponOwner) : nullptr;
	const bool bIsLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();

	if (bIsLocallyControlled)
	{
		// FPS: Play on FPSMesh (visible to owner)
		if (FPSMesh)
		{
			if (UAnimInstance* FPSAnimInstance = FPSMesh->GetAnimInstance())
			{
				FPSAnimInstance->Montage_Play(SlideShootMontage);
			}
		}
	}
	else
	{
		// TPS: Play on TPSMesh (visible to others)
		if (TPSMesh)
		{
			if (UAnimInstance* TPSAnimInstance = TPSMesh->GetAnimInstance())
			{
				TPSAnimInstance->Montage_Play(SlideShootMontage);
			}
		}
	}
}

void AHKVP9::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// Call base implementation (triggers Multicast + Client RPCs for muzzle VFX and character anims)
	Super::HandleShotFired_Implementation(MuzzleLocation, Direction);

	// VP9-specific: Check if magazine is empty after this shot → slide locks back (SERVER ONLY)
	if (HasAuthority() && CurrentMagazine && CurrentMagazine->CurrentAmmo == 0)
	{
		bSlideLockedBack = true;
		// Note: bSlideLockedBack is REPLICATED, OnRep will update clients
		// Server must also update locally since OnRep doesn't run on server
		PropagateStateToAnimInstances();
	}
}

void AHKVP9::OnUnequipped_Implementation()
{
	// Call base implementation (cancels reload, resets aiming)
	Super::OnUnequipped_Implementation();

	// VP9-specific: Reset state (slide goes forward)
	bSlideLockedBack = false;

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

void AHKVP9::OnWeaponReloadComplete_Implementation()
{
	// Call base implementation
	Super::OnWeaponReloadComplete_Implementation();

	// VP9-specific: Reset slide state (slide goes forward after reload)
	bSlideLockedBack = false;

	// Server: propagate state to AnimInstances immediately
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		PropagateStateToAnimInstances();
	}
}

