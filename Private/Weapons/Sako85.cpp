// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/Sako85.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/BoltActionFireComponent.h"
#include "Components/BoltActionReloadComponent.h"
#include "Components/BallisticsComponent.h"
#include "Net/UnrealNetwork.h"

ASako85::ASako85()
{
	// ============================================
	// WEAPON INFO
	// ============================================
	Name = FText::FromString("Sako 85");
	Description = FText::FromString(".308 Winchester bolt-action precision rifle. High accuracy, manual bolt cycling between shots.");

	// ============================================
	// CALIBER
	// ============================================
	AcceptedCaliberType = EAmmoCaliberType::NATO_762x51mm;  // .308 Win / 7.62x51mm NATO

	// ============================================
	// COMPONENTS
	// ============================================

	// Bolt-Action Fire Component
	BoltActionFireComponent = CreateDefaultSubobject<UBoltActionFireComponent>(TEXT("BoltActionFireComponent"));
	BoltActionFireComponent->FireRate = 40.0f;       // ~40 RPM (bolt-action limited)
	BoltActionFireComponent->SpreadScale = 0.3f;     // High accuracy (sniper rifle)
	BoltActionFireComponent->RecoilScale = 1.5f;     // Higher recoil (.308 is powerful)

	// Assign to BaseWeapon's FireComponent pointer for interface compatibility
	FireComponent = BoltActionFireComponent;

	// Bolt-Action Reload Component
	BoltActionReloadComponent = CreateDefaultSubobject<UBoltActionReloadComponent>(TEXT("BoltActionReloadComponent"));
	BoltActionReloadComponent->MagazineOutSocketName = FName("weapon_l");
	BoltActionReloadComponent->MagazineInSocketName = FName("magazine");
	BoltActionReloadComponent->bReattachDuringReload = true;

	// Assign to BaseWeapon's ReloadComponent pointer for interface compatibility
	ReloadComponent = BoltActionReloadComponent;

	// Ballistics Component (standard single-projectile)
	BallisticsComponent = CreateDefaultSubobject<UBallisticsComponent>(TEXT("BallisticsComponent"));

	// ============================================
	// ATTACHMENT SOCKETS
	// ============================================
	CharacterAttachSocket = FName("weapon_r");     // Right hand (normal hold)
	ReloadAttachSocket = FName("weapon_l");        // Left hand support during reload/bolt-action

	// ============================================
	// AIMING DEFAULTS (Sniper rifle settings)
	// ============================================
	AimFOV = 30.0f;              // Narrow FOV for scope zoom
	AimLookSpeed = 0.3f;         // Slow look speed when scoped
	LeaningScale = 0.5f;         // Reduced leaning (stable platform)
	BreathingScale = 1.5f;       // More noticeable breathing sway (precision matters)
}

// ============================================
// REPLICATION
// ============================================

void ASako85::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Sako85 state is managed by BoltActionFireComponent
	// which handles its own replication (bIsCyclingBolt, bChamberEmpty)
}

// ============================================
// BASEWEAPON OVERRIDES
// ============================================

bool ASako85::CanBeUnequipped_Implementation() const
{
	// First check base conditions (not reloading, no montage playing)
	if (!Super::CanBeUnequipped_Implementation())
	{
		return false;
	}

	// Bolt-action specific: Cannot unequip while bolt is cycling
	if (BoltActionFireComponent && BoltActionFireComponent->IsCyclingBolt())
	{
		return false;
	}

	// Cannot unequip while bolt-action pending after shoot
	if (BoltActionFireComponent && BoltActionFireComponent->IsBoltActionPendingAfterShoot())
	{
		return false;
	}

	return true;
}

void ASako85::OnUnequipped_Implementation()
{
	// Call base implementation (cancels reload, resets aiming)
	Super::OnUnequipped_Implementation();

	// SERVER ONLY: Reset Sako85-specific bolt-action state
	// Clients receive state via component replication
	if (HasAuthority())
	{
		if (BoltActionFireComponent)
		{
			// Reset chamber state on unequip (internal component access)
			BoltActionFireComponent->ResetChamberState();
		}
	}
}

// ============================================
// RELOADABLE INTERFACE OVERRIDE
// ============================================

void ASako85::OnWeaponReloadComplete_Implementation()
{
	// Call base implementation
	Super::OnWeaponReloadComplete_Implementation();

	// NOTE: BoltActionReloadComponent handles bolt-action after reload
	// Chamber state is reset in OnBoltActionAfterReloadComplete()
	// This callback is for any additional Sako85-specific behavior
}
