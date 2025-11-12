// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/FireComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"

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

	// Calculate random spread variation
	float RandomSpread = FMath::RandRange(RandomSpreadMin, RandomSpreadMax);

	// Calculate movement-based spread
	float MovementSpread = 0.0f;
	if (AActor* Owner = GetOwner())
	{
		// Get owner velocity
		FVector Velocity = Owner->GetVelocity();
		float Speed = Velocity.Size();

		// Scale spread by movement speed (cm/s to spread degrees)
		// Formula: (Speed / 100.0) * Multiplier
		MovementSpread = (Speed / 100.0f) * MovementSpreadMultiplier;
	}

	// Combine all spread components
	// ConeHalfAngle = Recoil + Spread + RandomSpread + MovementSpread
	float TotalSpread = Recoil + Spread + RandomSpread + MovementSpread;

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

	// 2. Call BallisticsComponent to shoot
	if (BallisticsComponent)
	{
		BallisticsComponent->Shoot(ViewLocation, ViewDirection);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FireComponent::Fire() - BallisticsComponent is null!"));
	}

	// 3. Apply recoil
	ApplyRecoil();
}

void UFireComponent::ApplyRecoil()
{
	// TODO: Implement recoil application
	// - Apply camera shake
	// - Apply weapon mesh rotation/offset
	// - Use RecoilScale property

	UE_LOG(LogTemp, Verbose, TEXT("FireComponent::ApplyRecoil() - Recoil: %.2f"), RecoilScale);
}
