// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/FireComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/RecoilHandlerInterface.h"

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
	// Check ballistics component is valid
	if (!BallisticsComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::CanFire() - BallisticsComponent is null!"));
		return false;
	}

	// ✅ Check ammo via IAmmoConsumerInterface
	AActor* WeaponActor = GetOwner(); // BaseWeapon
	if (WeaponActor && WeaponActor->Implements<UAmmoConsumerInterface>())
	{
		int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(WeaponActor);

		if (CurrentAmmo <= 0)
		{
			UE_LOG(LogTemp, Verbose, TEXT("FireComponent::CanFire() - No ammo (Clip: %d)"), CurrentAmmo);
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::CanFire() - Owner does not implement IAmmoConsumerInterface!"));
		return false;
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

	// Hardcoded common spread constants (not exposed in defaults)
	const float BaseSpread = 0.5f;           // Base spread cone angle (degrees)
	const float RandomSpreadMin = 0.0f;      // Random variation min (degrees)
	const float RandomSpreadMax = 1.0f;      // Random variation max (degrees)
	const float MovementSpreadMultiplier = 2.0f; // Movement penalty multiplier (3x spread at full sprint)

	// Get normalized movement velocity (0.0 = standing, 1.0 = sprint speed)
	float NormalizedVelocity = 0.0f;

	// FireComponent owner is BaseWeapon, BaseWeapon owner is Character
	AActor* WeaponActor = GetOwner(); // BaseWeapon
	if (WeaponActor)
	{
		AActor* CharacterActor = WeaponActor->GetOwner(); // Character
		if (CharacterActor)
		{
			FVector Velocity = CharacterActor->GetVelocity();
			float Speed = Velocity.Size();

			// Normalize by max sprint speed (650 cm/s)
			// Result: 0.0 = standing, 1.0 = sprinting, >1.0 clamped
			NormalizedVelocity = FMath::Clamp(Speed / 650.0f, 0.0f, 1.0f);
		}
	}

	// Calculate base random spread (not affected by movement yet)
	float RandomSpread = FMath::RandRange(RandomSpreadMin, RandomSpreadMax);

	// Check if weapon is aiming (ADS disables movement penalty)
	bool bIsAiming = false;
	if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
	{
		// Get IsAiming state via IHoldableInterface
		bIsAiming = IHoldableInterface::Execute_GetIsAiming(WeaponActor);
	}

	// Calculate movement penalty (1.0 at standing, 3.0 at full sprint)
	// When aiming (ADS), movement penalty is disabled (always 1.0)
	float MovementPenalty = 1.0f;
	if (!bIsAiming)
	{
		// Hip-fire: Apply movement penalty
		MovementPenalty = 1.0f + (NormalizedVelocity * MovementSpreadMultiplier);
	}

	// Apply movement penalty to combined spread, then apply SpreadScale multiplier
	// ADS: TotalSpread = (BaseSpread + RandomSpread) * 1.0 * SpreadScale
	// Hip: TotalSpread = (BaseSpread + RandomSpread) * MovementPenalty * SpreadScale
	float TotalSpread = (BaseSpread + RandomSpread) * MovementPenalty * SpreadScale;

	// Convert total spread from degrees to radians
	float SpreadRadians = FMath::DegreesToRadians(TotalSpread);

	// Generate random direction within cone
	FVector SpreadDirection = UKismetMathLibrary::RandomUnitVectorInConeInRadians(Direction, SpreadRadians);

	return SpreadDirection;
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

	// Get weapon owner (character/pawn)
	// GetOwner() returns BaseWeapon, then we get the weapon's owner
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor)
	{
		UE_LOG(LogTemp, Error, TEXT("FireComponent::Fire() - WeaponActor is null!"));
		return;
	}

	AActor* WeaponOwner = WeaponActor->GetOwner();
	if (!WeaponOwner)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::Fire() - Weapon has no owner!"));
		return;
	}

	// Get view center point (camera/eyes location) and direction
	FVector ViewLocation;
	FRotator ViewRotation;

	// ✅ Component-based view point retrieval
	// Check if owner implements IViewPointProvider interface (e.g., AFPSCharacter)
	if (WeaponOwner->Implements<UViewPointProviderInterface>())
	{
		// ✅ Use interface to get ACCURATE view point (includes custom replicated Pitch)
		IViewPointProviderInterface::Execute_GetShootingViewPoint(WeaponOwner, ViewLocation, ViewRotation);
	}
	else
	{
		// ❌ Fallback: Use default GetActorEyesViewPoint() (may have incorrect Pitch)
		WeaponOwner->GetActorEyesViewPoint(ViewLocation, ViewRotation);

		UE_LOG(LogTemp, Warning, TEXT("FireComponent::Fire() - Owner '%s' does not implement IViewPointProvider!"),
			*WeaponOwner->GetName());
	}

	// Convert rotation to direction vector
	FVector ViewDirection = ViewRotation.Vector();

	// 1. Consume ammo via IAmmoConsumerInterface
	if (WeaponActor->Implements<UAmmoConsumerInterface>())
	{
		FUseContext Ctx;
		Ctx.Controller = WeaponOwner->GetInstigatorController();
		Ctx.Pawn = Cast<APawn>(WeaponOwner);

		int32 Consumed = IAmmoConsumerInterface::Execute_ConsumeAmmo(WeaponActor, 1, Ctx);

		if (Consumed <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("FireComponent::Fire() - Failed to consume ammo!"));
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FireComponent::Fire() - Owner does not implement IAmmoConsumerInterface!"));
		return;
	}

	// 2. Apply spread to direction
	FVector SpreadDirection = ApplySpread(ViewDirection);

	// 3. Call BallisticsComponent to shoot
	if (BallisticsComponent)
	{
		BallisticsComponent->Shoot(ViewLocation, SpreadDirection);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FireComponent::Fire() - BallisticsComponent is null!"));
	}

	// 4. Apply recoil
	ApplyRecoil();
}

void UFireComponent::ApplyRecoil()
{
	// SERVER ONLY (called from Fire())
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::ApplyRecoil() - Called on client! Recoil applies on server only."));
		return;
	}

	// Get weapon owner (Character/Pawn)
	AActor* WeaponActor = GetOwner();  // BaseWeapon
	if (!WeaponActor)
	{
		UE_LOG(LogTemp, Error, TEXT("FireComponent::ApplyRecoil() - WeaponActor is null!"));
		return;
	}

	AActor* WeaponOwner = WeaponActor->GetOwner();  // Character/Pawn
	if (!WeaponOwner)
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::ApplyRecoil() - Weapon has no owner!"));
		return;
	}

	// ✅ Interface communication (NO cast to AFPSCharacter!)
	// Follows same pattern as IBallisticsHandlerInterface
	if (WeaponOwner->Implements<URecoilHandlerInterface>())
	{
		// Call interface method - server will broadcast to all clients
		IRecoilHandlerInterface::Execute_ApplyRecoilKick(WeaponOwner, RecoilScale);

		UE_LOG(LogTemp, Verbose, TEXT("FireComponent::ApplyRecoil() - Recoil applied via interface: Scale=%.2f"), RecoilScale);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FireComponent::ApplyRecoil() - Owner '%s' does not implement IRecoilHandlerInterface!"),
			*WeaponOwner->GetName());
	}
}
