// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/Spas12.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/SemiAutoFireComponent.h"
#include "Components/PumpActionReloadComponent.h"
#include "Components/ShotgunBallisticsComponent.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Net/UnrealNetwork.h"

ASpas12::ASpas12()
{
	// ============================================
	// WEAPON INFO
	// ============================================
	Name = FText::FromString("SPAS-12");
	Description = FText::FromString("12 Gauge semi-auto combat shotgun. High damage at close range, shell-by-shell reload.");

	// ============================================
	// CALIBER
	// ============================================
	AcceptedCaliberType = EAmmoCaliberType::Gauge_12;

	// ============================================
	// COMPONENTS
	// ============================================

	// Semi-Auto Fire Component (no pump-action during shooting)
	SemiAutoFireComponent = CreateDefaultSubobject<USemiAutoFireComponent>(TEXT("SemiAutoFireComponent"));
	SemiAutoFireComponent->FireRate = 300.0f;        // ~300 RPM (semi-auto)
	SemiAutoFireComponent->SpreadScale = 1.5f;       // Wide spread (shotgun buckshot)
	SemiAutoFireComponent->RecoilScale = 2.0f;       // Heavy recoil (12 gauge)

	// Assign to BaseWeapon's FireComponent pointer for interface compatibility
	FireComponent = SemiAutoFireComponent;

	// Pump-Action Reload Component (shell-by-shell reload)
	PumpActionReloadComponent = CreateDefaultSubobject<UPumpActionReloadComponent>(TEXT("PumpActionReloadComponent"));
	PumpActionReloadComponent->bReattachDuringReload = true;

	// Assign to BaseWeapon's ReloadComponent pointer for interface compatibility
	ReloadComponent = PumpActionReloadComponent;

	// Shotgun Ballistics Component (multi-pellet with cone spread)
	// Uses base class BallisticsComponent pointer - no separate UPROPERTY needed
	UShotgunBallisticsComponent* ShotgunBallistics = CreateDefaultSubobject<UShotgunBallisticsComponent>(TEXT("BallisticsComponent"));
	ShotgunBallistics->PelletCount = 9;           // 00 buckshot (9 pellets)
	ShotgunBallistics->PelletSpreadAngle = 5.0f;  // 5 degree cone spread

	// Assign to BaseWeapon's BallisticsComponent pointer for interface compatibility
	BallisticsComponent = ShotgunBallistics;

	// ============================================
	// ATTACHMENT SOCKETS
	// ============================================
	CharacterAttachSocket = FName("weapon_r");     // Right hand (normal hold)
	ReloadAttachSocket = FName("weapon_l");        // Left hand support during reload

	// ============================================
	// AIMING DEFAULTS (Shotgun settings)
	// ============================================
	AimFOV = 55.0f;              // Moderate FOV (iron sights)
	AimLookSpeed = 0.6f;         // Medium look speed
	LeaningScale = 0.8f;         // Reduced leaning (heavy weapon)
	BreathingScale = 0.8f;       // Moderate sway (heavy weapon)

	// ============================================
	// SPAS-12 STATE DEFAULTS
	// ============================================
	BoltCarrierOpen = false;
}

// ============================================
// REPLICATION
// ============================================

void ASpas12::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASpas12, BoltCarrierOpen);
}

void ASpas12::OnRep_BoltCarrierOpen()
{
	ForceUpdateWeaponAnimInstances();
}

// ============================================
// BASEWEAPON OVERRIDES
// ============================================

void ASpas12::Multicast_PlayShootEffects_Implementation()
{
	// Call base implementation (muzzle VFX + character shoot anims)
	Super::Multicast_PlayShootEffects_Implementation();

	// SPAS-12-specific: Play bolt carrier shoot montage on weapon meshes
	// This runs on ALL clients (server + remote clients)
	PlayWeaponMontage(BoltCarrierShootMontage);
}

void ASpas12::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// Call base implementation (triggers Multicast_PlayShootEffects for character anims + muzzle VFX)
	Super::HandleShotFired_Implementation(MuzzleLocation, Direction);

	// SERVER ONLY: Check if magazine is empty after this shot → bolt locks back
	if (HasAuthority())
	{
		// Use IAmmoConsumerInterface to check ammo (Golden Rule compliance)
		if (Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(const_cast<ASpas12*>(this));
			if (CurrentAmmo == 0)
			{
				BoltCarrierOpen = true;
				ForceUpdateWeaponAnimInstances();

				// Also set chamber empty on reload component
				if (PumpActionReloadComponent)
				{
					PumpActionReloadComponent->SetChamberEmpty(true);
				}
			}
		}
	}
}

void ASpas12::OnUnequipped_Implementation()
{
	// Call base implementation (cancels reload, resets aiming)
	Super::OnUnequipped_Implementation();

	// SERVER ONLY: Reset SPAS-12 specific state
	// Clients receive state via OnRep which calls PropagateStateToAnimInstances
	if (HasAuthority())
	{
		// Reset bolt carrier state
		BoltCarrierOpen = false;

		// Reset chamber state on reload component (internal component access)
		if (PumpActionReloadComponent)
		{
			PumpActionReloadComponent->ResetChamberState();
		}

		// Propagate state to AnimInstances immediately on server
		ForceUpdateWeaponAnimInstances();
	}
}

// ============================================
// RELOADABLE INTERFACE OVERRIDE
// ============================================

void ASpas12::OnWeaponReloadComplete_Implementation()
{
	// Call base implementation
	Super::OnWeaponReloadComplete_Implementation();

	// SERVER ONLY: Reset bolt carrier state (bolt goes forward after reload)
	// Clients receive state via OnRep which calls ForceUpdateWeaponAnimInstances
	if (HasAuthority())
	{
		BoltCarrierOpen = false;
		ForceUpdateWeaponAnimInstances();
	}
}
