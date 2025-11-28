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

	// DEBUG: Log context
	AActor* WeaponOwner = GetOwner();
	APawn* OwnerPawn = WeaponOwner ? Cast<APawn>(WeaponOwner) : nullptr;
	const bool bIsLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();

	UE_LOG(LogTemp, Warning, TEXT("[HKVP9] Multicast_PlayShootEffects - NetMode=%d | IsLocallyControlled=%d | HasAuthority=%d | Owner=%s"),
		(int32)GetNetMode(),
		bIsLocallyControlled,
		HasAuthority(),
		WeaponOwner ? *WeaponOwner->GetName() : TEXT("nullptr"));

	// VP9-specific: Play slide shoot montage on weapon meshes
	// Play on BOTH meshes - visibility handled by OnlyOwnerSee/OwnerNoSee settings
	// (Same pattern as M4A1::PlayWeaponMontage)
	if (!SlideShootMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("[HKVP9] SlideShootMontage is nullptr!"));
		return;
	}

	// Play on FPS mesh (OnlyOwnerSee - visible to owner only)
	if (FPSMesh)
	{
		UAnimInstance* FPSAnimInstance = FPSMesh->GetAnimInstance();

		// Extended debug info
		USceneComponent* AttachParent = FPSMesh->GetAttachParent();
		AActor* AttachOwner = AttachParent ? AttachParent->GetOwner() : nullptr;

		UE_LOG(LogTemp, Warning, TEXT("[HKVP9] FPSMesh: Valid=%d | AnimInstance=%s | AnimClass=%s | IsVisible=%d | OnlyOwnerSee=%d | HiddenInGame=%d"),
			true,
			FPSAnimInstance ? *FPSAnimInstance->GetClass()->GetName() : TEXT("nullptr"),
			FPSMesh->GetAnimClass() ? *FPSMesh->GetAnimClass()->GetName() : TEXT("nullptr"),
			FPSMesh->IsVisible(),
			FPSMesh->bOnlyOwnerSee,
			FPSMesh->bHiddenInGame);

		UE_LOG(LogTemp, Warning, TEXT("[HKVP9] FPSMesh AttachParent=%s | AttachParentOwner=%s | ComponentTick=%d"),
			AttachParent ? *AttachParent->GetName() : TEXT("nullptr"),
			AttachOwner ? *AttachOwner->GetName() : TEXT("nullptr"),
			FPSMesh->PrimaryComponentTick.IsTickFunctionEnabled());

		if (FPSAnimInstance)
		{
			// Check if there's already a montage playing in same slot
			FName SlotName = SlideShootMontage->GetGroupName();
			UAnimMontage* CurrentMontage = FPSAnimInstance->GetCurrentActiveMontage();

			UE_LOG(LogTemp, Warning, TEXT("[HKVP9] FPSMesh Before Play: CurrentMontage=%s | SlotName=%s"),
				CurrentMontage ? *CurrentMontage->GetName() : TEXT("nullptr"),
				*SlotName.ToString());

			float Duration = FPSAnimInstance->Montage_Play(SlideShootMontage);

			// Check again after play
			UAnimMontage* AfterMontage = FPSAnimInstance->GetCurrentActiveMontage();
			bool bIsPlaying = FPSAnimInstance->Montage_IsPlaying(SlideShootMontage);

			UE_LOG(LogTemp, Warning, TEXT("[HKVP9] FPSMesh After Play: Duration=%.3f | CurrentMontage=%s | IsPlaying=%d"),
				Duration,
				AfterMontage ? *AfterMontage->GetName() : TEXT("nullptr"),
				bIsPlaying);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[HKVP9] FPSMesh->GetAnimInstance() returned nullptr!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[HKVP9] FPSMesh is nullptr!"));
	}

	// Play on TPS mesh (OwnerNoSee - visible to others)
	if (TPSMesh)
	{
		UAnimInstance* TPSAnimInstance = TPSMesh->GetAnimInstance();
		UE_LOG(LogTemp, Warning, TEXT("[HKVP9] TPSMesh: Valid=%d | AnimInstance=%s | AnimClass=%s | IsVisible=%d | OwnerNoSee=%d"),
			true,
			TPSAnimInstance ? *TPSAnimInstance->GetClass()->GetName() : TEXT("nullptr"),
			TPSMesh->GetAnimClass() ? *TPSMesh->GetAnimClass()->GetName() : TEXT("nullptr"),
			TPSMesh->IsVisible(),
			TPSMesh->bOwnerNoSee);

		if (TPSAnimInstance)
		{
			float Duration = TPSAnimInstance->Montage_Play(SlideShootMontage);
			UE_LOG(LogTemp, Warning, TEXT("[HKVP9] TPSMesh Montage_Play returned Duration=%.3f"), Duration);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[HKVP9] TPSMesh->GetAnimInstance() returned nullptr!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[HKVP9] TPSMesh is nullptr!"));
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

