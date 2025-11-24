// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/FireComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/RecoilHandlerInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Animation/AnimInstance.h"

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
}

void UFireComponent::TriggerReleased()
{
	bTriggerHeld = false;
}

bool UFireComponent::CanFire() const
{
	if (!BallisticsComponent)
	{
		return false;
	}

	AActor* WeaponActor = GetOwner();
	if (WeaponActor && WeaponActor->Implements<UAmmoConsumerInterface>())
	{
		int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(WeaponActor);

		if (CurrentAmmo <= 0)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	AActor* CharacterOwner = WeaponActor ? WeaponActor->GetOwner() : nullptr;
	if (CharacterOwner && CharacterOwner->Implements<UCharacterMeshProviderInterface>())
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterOwner);
		if (BodyMesh)
		{
			UAnimInstance* AnimInst = BodyMesh->GetAnimInstance();
			if (AnimInst && AnimInst->IsSlotActive("Shoot"))
			{
				return false;
			}
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

	// Hardcoded common spread constants (not exposed in defaults)
	const float BaseSpread = 0.5f;           // Base spread cone angle (degrees)
	const float RandomSpreadMin = 0.0f;      // Random variation min (degrees)
	const float RandomSpreadMax = 1.0f;      // Random variation max (degrees)
	const float MovementSpreadMultiplier = 2.0f; // Movement penalty multiplier (3x spread at full sprint)
	const float RecoilSpreadMultiplier = 1.5f;   // Recoil penalty multiplier (2.5x spread at sustained fire)

	// Get normalized movement velocity (0.0 = standing, 1.0 = sprint speed)
	float NormalizedVelocity = 0.0f;
	bool bIsAiming = false;
	float RecoilFactor = 0.0f;

	// FireComponent owner is BaseWeapon, BaseWeapon owner is Character
	AActor* WeaponActor = GetOwner(); // BaseWeapon
	if (WeaponActor)
	{
		AActor* CharacterActor = WeaponActor->GetOwner(); // Character
		if (CharacterActor)
		{
			// Get velocity
			FVector Velocity = CharacterActor->GetVelocity();
			float Speed = Velocity.Size();

			// Normalize by max sprint speed (650 cm/s)
			// Result: 0.0 = standing, 1.0 = sprinting, >1.0 clamped
			NormalizedVelocity = FMath::Clamp(Speed / 650.0f, 0.0f, 1.0f);

			// ✅ CAPABILITY-BASED: Use interface, not direct cast
			// Get IsAiming state via IHoldableInterface on Weapon (local state is OK here for spread visual)
			if (WeaponActor->Implements<UHoldableInterface>())
			{
				bIsAiming = IHoldableInterface::Execute_GetIsAiming(WeaponActor);
			}

			// ✅ CAPABILITY-BASED: Use IViewPointProviderInterface for recoil factor
			// No direct cast to AFPSCharacter, no direct component access
			// Character provides recoil factor via interface
			if (CharacterActor->Implements<UViewPointProviderInterface>())
			{
				RecoilFactor = IViewPointProviderInterface::Execute_GetRecoilFactor(CharacterActor);
			}
		}
	}

	// Calculate base random spread (not affected by movement yet)
	float RandomSpread = FMath::RandRange(RandomSpreadMin, RandomSpreadMax);

	// Calculate movement penalty (1.0 at standing, 3.0 at full sprint)
	// When aiming (ADS), movement penalty is disabled (always 1.0)
	float MovementPenalty = 1.0f;
	if (!bIsAiming)
	{
		// Hip-fire: Apply movement penalty
		MovementPenalty = 1.0f + (NormalizedVelocity * MovementSpreadMultiplier);
	}

	// Calculate recoil penalty (1.0 no shots, 2.5x at sustained fire)
	// Recoil penalty applies regardless of ADS state (sustained fire reduces accuracy)
	float RecoilPenalty = 1.0f + (RecoilFactor * RecoilSpreadMultiplier);

	// Apply all penalties: movement (if hip-fire) + recoil + SpreadScale multiplier
	// Example scenarios:
	// - Standing, ADS, no shots: (0.5 + rand) * 1.0 * 1.0 * SpreadScale
	// - Sprint, Hip, no shots:   (0.5 + rand) * 3.0 * 1.0 * SpreadScale
	// - Standing, Hip, 8 shots:  (0.5 + rand) * 1.0 * 2.5 * SpreadScale
	// - Sprint, Hip, 8 shots:    (0.5 + rand) * 3.0 * 2.5 * SpreadScale = 7.5x multiplier!
	float TotalSpread = (BaseSpread + RandomSpread) * MovementPenalty * RecoilPenalty * SpreadScale;

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
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!CanFire())
	{
		return;
	}

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor)
	{
		return;
	}

	AActor* WeaponOwner = WeaponActor->GetOwner();
	if (!WeaponOwner)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;

	if (WeaponOwner->Implements<UViewPointProviderInterface>())
	{
		IViewPointProviderInterface::Execute_GetShootingViewPoint(WeaponOwner, ViewLocation, ViewRotation);
	}
	else
	{
		WeaponOwner->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	FVector ViewDirection = ViewRotation.Vector();

	if (WeaponActor->Implements<UAmmoConsumerInterface>())
	{
		FUseContext Ctx;
		Ctx.Controller = WeaponOwner->GetInstigatorController();
		// Cast to APawn is required by FUseContext structure (Pawn field is APawn*)
		// This is a framework requirement, not an architectural violation
		Ctx.Pawn = Cast<APawn>(WeaponOwner);

		int32 Consumed = IAmmoConsumerInterface::Execute_ConsumeAmmo(WeaponActor, 1, Ctx);

		if (Consumed <= 0)
		{
			return;
		}
	}
	else
	{
		return;
	}

	FVector SpreadDirection = ApplySpread(ViewDirection);

	if (BallisticsComponent)
	{
		BallisticsComponent->Shoot(ViewLocation, SpreadDirection);
	}

	// 4. Apply recoil
	ApplyRecoil();
}

void UFireComponent::ApplyRecoil()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor)
	{
		return;
	}

	AActor* WeaponOwner = WeaponActor->GetOwner();
	if (!WeaponOwner)
	{
		return;
	}

	if (WeaponOwner->Implements<URecoilHandlerInterface>())
	{
		IRecoilHandlerInterface::Execute_ApplyRecoilKick(WeaponOwner, RecoilScale);
	}
}
