// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/FireComponent.h"
#include "Components/BallisticsComponent.h"
#include "BaseMagazine.h"
#include "BaseWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

UFireComponent::UFireComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// NOT replicated - fire logic runs on server via RPC
	SetIsReplicatedByDefault(false);
}

void UFireComponent::BeginPlay()
{
	Super::BeginPlay();

	// Local initialization
	// References (CurrentMagazine, BallisticsComponent) will be set by BaseWeapon
}

// ============================================
// FIRE CONTROL API
// ============================================

void UFireComponent::TriggerPulled()
{
	// Base implementation - override in subclasses
	UE_LOG(LogTemp, Warning, TEXT("FireComponent::TriggerPulled() - Called on abstract base class! Override in subclass."));
}

void UFireComponent::TriggerReleased()
{
	// Base implementation - override in subclasses
	bTriggerHeld = false;
}

bool UFireComponent::CanFire() const
{
	// Check magazine has ammo
	if (!CurrentMagazine || CurrentMagazine->CurrentAmmo <= 0)
	{
		return false;
	}

	// Check ballistics component is valid
	if (!BallisticsComponent)
	{
		return false;
	}

	// Check weapon is not reloading
	if (ABaseWeapon* Weapon = Cast<ABaseWeapon>(GetOwner()))
	{
		if (Weapon->IsReload)
		{
			return false;
		}
	}

	return true;
}

// ============================================
// FIRE HELPERS
// ============================================

FVector UFireComponent::ApplySpread(FVector Direction) const
{
	// Normalize input direction
	Direction.Normalize();

	// Convert spread from degrees to radians
	float SpreadRadians = FMath::DegreesToRadians(Spread);

	// Generate random spread within cone
	// Using random cone distribution for realistic spread pattern
	FVector SpreadDirection = UKismetMathLibrary::RandomUnitVectorInConeInRadians(Direction, SpreadRadians);

	return SpreadDirection;
}

FVector UFireComponent::GetMuzzleLocation() const
{
	ABaseWeapon* Weapon = Cast<ABaseWeapon>(GetOwner());
	if (!Weapon)
	{
		return FVector::ZeroVector;
	}

	// Get FPS mesh (owner sees this)
	USkeletalMeshComponent* WeaponMesh = Weapon->FPSMesh;
	if (!WeaponMesh)
	{
		return FVector::ZeroVector;
	}

	// Get muzzle socket location
	if (WeaponMesh->DoesSocketExist(FName("muzzle")))
	{
		return WeaponMesh->GetSocketLocation(FName("muzzle"));
	}

	// Fallback: Weapon mesh origin
	return WeaponMesh->GetComponentLocation();
}

FVector UFireComponent::GetMuzzleDirection() const
{
	ABaseWeapon* Weapon = Cast<ABaseWeapon>(GetOwner());
	if (!Weapon)
	{
		return FVector::ForwardVector;
	}

	// Get FPS mesh (owner sees this)
	USkeletalMeshComponent* WeaponMesh = Weapon->FPSMesh;
	if (!WeaponMesh)
	{
		return FVector::ForwardVector;
	}

	// Get muzzle socket rotation
	if (WeaponMesh->DoesSocketExist(FName("muzzle")))
	{
		FRotator MuzzleRotation = WeaponMesh->GetSocketRotation(FName("muzzle"));
		return MuzzleRotation.Vector();
	}

	// Fallback: Weapon mesh forward
	return WeaponMesh->GetForwardVector();
}

// ============================================
// FIRE IMPLEMENTATION
// ============================================

void UFireComponent::Fire()
{
	// SERVER ONLY
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::Fire() - Called on client! Fire logic runs on server only."));
		return;
	}

	// Check can fire
	if (!CanFire())
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::Fire() - CanFire() returned false!"));
		return;
	}

	// 1. Consume ammo
	ConsumeAmmo();

	// 2. Get muzzle location and direction
	FVector MuzzleLocation = GetMuzzleLocation();
	FVector MuzzleDirection = GetMuzzleDirection();

	// 3. Apply spread
	FVector Direction = ApplySpread(MuzzleDirection);

	// 4. Call BallisticsComponent->Shoot()
	if (BallisticsComponent)
	{
		BallisticsComponent->Shoot(MuzzleLocation, Direction);
	}

	// 5. Apply recoil
	ApplyRecoil();

	UE_LOG(LogTemp, Log, TEXT("FireComponent::Fire() - Shot fired! Ammo remaining: %d"),
		CurrentMagazine ? CurrentMagazine->CurrentAmmo : 0);
}

void UFireComponent::ConsumeAmmo()
{
	if (CurrentMagazine)
	{
		CurrentMagazine->RemoveAmmo();
	}
}

void UFireComponent::ApplyRecoil()
{
	// TODO: Implement recoil application
	// - Apply camera shake
	// - Apply weapon mesh rotation/offset
	// - Use RecoilScale property

	UE_LOG(LogTemp, Verbose, TEXT("FireComponent::ApplyRecoil() - Recoil: %.2f"), RecoilScale);
}
