// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BallisticsComponent.h"
#include "Data/AmmoTypeDataAsset.h"

UBallisticsComponent::UBallisticsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// NOT replicated - this is a pure utility component
	SetIsReplicatedByDefault(false);
}

void UBallisticsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Local initialization (runs on all machines)
	// Component is passive utility for ballistic calculations
}

// ============================================
// SHOOT API
// ============================================

void UBallisticsComponent::Shoot(FVector Location, FVector Direction)
{
	// SERVER ONLY - projectile spawning
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("BallisticsComponent::Shoot() - Called on client! Projectiles spawn on server only."));
		return;
	}

	// Load caliber data
	UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous();
	if (!CaliberData)
	{
		UE_LOG(LogTemp, Error, TEXT("BallisticsComponent::Shoot() - No CaliberDataAsset assigned!"));
		return;
	}

	// Log shoot event
	UE_LOG(LogTemp, Log, TEXT("BallisticsComponent::Shoot() - Location: %s, Direction: %s, Caliber: %s"),
		*Location.ToString(),
		*Direction.ToString(),
		*CaliberData->AmmoName.ToString());

	// ============================================
	// TODO: PROJECTILE SPAWNING (Next step)
	// ============================================
	// 1. Spawn projectile actor at Location
	// 2. Apply initial velocity: Direction * MuzzleVelocity
	// 3. Set projectile properties from CaliberData:
	//    - Mass (for physics simulation)
	//    - Drag coefficient (for bullet drop)
	//    - Damage
	//    - Penetration power
	//    - Impact VFX map
	// 4. Enable physics simulation with custom trajectory

	// ============================================
	// TEMPORARY: Visual feedback for testing
	// ============================================
	if (GEngine)
	{
		FString DebugText = FString::Printf(TEXT("SHOOT! Caliber: %s | Velocity: %.0f m/s | Damage: %.1f"),
			*CaliberData->AmmoName.ToString(),
			CaliberData->MuzzleVelocity,
			CaliberData->Damage);

		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, DebugText);
	}

	// TODO: Spawn muzzle flash VFX
	// TODO: Spawn shell ejection
	// TODO: Play fire sound
}

// ============================================
// BALLISTIC CALCULATIONS
// ============================================

float UBallisticsComponent::CalculateBulletDrop(float Distance) const
{
	UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous();
	if (!CaliberData)
	{
		return 0.0f;
	}

	// Simplified bullet drop calculation
	// drop = (g * distance^2) / (2 * velocity^2) * drag
	// g = 980 cm/s^2 (Unreal uses centimeters)
	const float Gravity = 980.0f;
	const float Velocity = CaliberData->MuzzleVelocity * 100.0f; // m/s to cm/s
	const float Drag = CaliberData->DragCoefficient;

	float Drop = (Gravity * Distance * Distance) / (2.0f * Velocity * Velocity);
	Drop *= Drag;

	return Drop;
}

float UBallisticsComponent::CalculateKineticEnergy() const
{
	UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous();
	if (!CaliberData)
	{
		return 0.0f;
	}

	// KE = 0.5 * mass * velocity^2
	// mass in grams, velocity in m/s
	// Result in joules
	float Mass = CaliberData->ProjectileMass / 1000.0f; // grams to kg
	float Velocity = CaliberData->MuzzleVelocity;

	return 0.5f * Mass * Velocity * Velocity;
}

// ============================================
// CALIBER DATA GETTERS
// ============================================

float UBallisticsComponent::GetProjectileMass() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->ProjectileMass;
	}
	return 0.0f;
}

float UBallisticsComponent::GetMuzzleVelocity() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->MuzzleVelocity;
	}
	return 0.0f;
}

float UBallisticsComponent::GetDragCoefficient() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->DragCoefficient;
	}
	return 0.0f;
}

float UBallisticsComponent::GetPenetrationPower() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->PenetrationPower;
	}
	return 0.0f;
}

float UBallisticsComponent::GetDamage() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->Damage;
	}
	return 0.0f;
}

float UBallisticsComponent::GetDamageRadius() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->DamageRadius;
	}
	return 0.0f;
}

EAmmoCaliberType UBallisticsComponent::GetCaliberType() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->CaliberType;
	}
	return EAmmoCaliberType::None;
}

FText UBallisticsComponent::GetAmmoName() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->AmmoName;
	}
	return FText::GetEmpty();
}

FName UBallisticsComponent::GetAmmoID() const
{
	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		return CaliberData->AmmoID;
	}
	return NAME_None;
}

// ============================================
// DEBUG
// ============================================

void UBallisticsComponent::DebugPrintCaliberData() const
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("BALLISTICS COMPONENT - CALIBER DATA"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));

	if (UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ammo Name: %s"), *CaliberData->AmmoName.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Ammo ID: %s"), *CaliberData->AmmoID.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Caliber Type: %s"), *UEnum::GetValueAsString(CaliberData->CaliberType));
		UE_LOG(LogTemp, Warning, TEXT(""));
		UE_LOG(LogTemp, Warning, TEXT("BALLISTICS:"));
		UE_LOG(LogTemp, Warning, TEXT("  Projectile Mass: %.2f g"), CaliberData->ProjectileMass);
		UE_LOG(LogTemp, Warning, TEXT("  Muzzle Velocity: %.1f m/s"), CaliberData->MuzzleVelocity);
		UE_LOG(LogTemp, Warning, TEXT("  Drag Coefficient: %.3f"), CaliberData->DragCoefficient);
		UE_LOG(LogTemp, Warning, TEXT("  Penetration Power: %.2f"), CaliberData->PenetrationPower);
		UE_LOG(LogTemp, Warning, TEXT("  Kinetic Energy: %.1f J"), CalculateKineticEnergy());
		UE_LOG(LogTemp, Warning, TEXT(""));
		UE_LOG(LogTemp, Warning, TEXT("DAMAGE:"));
		UE_LOG(LogTemp, Warning, TEXT("  Base Damage: %.1f"), CaliberData->Damage);
		UE_LOG(LogTemp, Warning, TEXT("  Damage Radius: %.1f cm"), CaliberData->DamageRadius);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No CaliberDataAsset assigned!"));
	}

	UE_LOG(LogTemp, Warning, TEXT("========================================"));

	// Also print to screen for easier debugging
	if (GEngine && CaliberDataAsset.LoadSynchronous())
	{
		UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous();
		FString DebugText = FString::Printf(TEXT("CALIBER: %s\nVelocity: %.0f m/s\nDamage: %.1f\nEnergy: %.1f J"),
			*CaliberData->AmmoName.ToString(),
			CaliberData->MuzzleVelocity,
			CaliberData->Damage,
			CalculateKineticEnergy());

		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, DebugText);
	}
}
