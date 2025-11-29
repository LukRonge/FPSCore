// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ShotgunBallisticsComponent.h"
#include "Data/AmmoTypeDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "Interfaces/BallisticsHandlerInterface.h"

UShotgunBallisticsComponent::UShotgunBallisticsComponent()
{
	// Default shotgun config (00 buckshot)
	PelletCount = 9;
	PelletSpreadAngle = 5.0f;
}

void UShotgunBallisticsComponent::Shoot(FVector Location, FVector Direction)
{
	// Server authority check
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!CurrentAmmoType)
	{
		return;
	}

	// ============================================
	// SINGLE NOTIFICATION (One muzzle flash per shot)
	// ============================================
	// Notify owner ONCE via IBallisticsHandlerInterface
	// This triggers single muzzle flash for all pellets
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->Implements<UBallisticsHandlerInterface>())
	{
		IBallisticsHandlerInterface::Execute_HandleShotFired(
			OwnerActor,
			FVector_NetQuantize(Location),
			FVector_NetQuantizeNormal(Direction)
		);
	}

	// ============================================
	// FIRE MULTIPLE PELLETS
	// ============================================
	// Each pellet gets independent spread and line trace
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; PelletIndex++)
	{
		FVector PelletDirection = ApplyPelletSpread(Direction);
		ShootPellet(Location, PelletDirection);
	}
}

FVector UShotgunBallisticsComponent::ApplyPelletSpread(const FVector& Direction) const
{
	if (PelletSpreadAngle <= 0.0f)
	{
		return Direction;
	}

	// Random angle within cone
	float HalfAngleRad = FMath::DegreesToRadians(PelletSpreadAngle * 0.5f);

	// Random point on unit sphere within cone
	// Use uniform distribution within cone for realistic spread pattern
	float RandomAngle = FMath::FRand() * 2.0f * PI;  // Azimuth [0, 2π]
	float RandomRadius = FMath::Sqrt(FMath::FRand()) * FMath::Sin(HalfAngleRad);  // Polar radius

	// Create offset in local space (Direction = forward)
	FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(FVector::RightVector, Direction).GetSafeNormal();
	}
	FVector Up = FVector::CrossProduct(Direction, Right);

	// Apply offset
	FVector Offset = Right * (RandomRadius * FMath::Cos(RandomAngle)) +
	                 Up * (RandomRadius * FMath::Sin(RandomAngle));

	// Final direction (normalized)
	return (Direction + Offset).GetSafeNormal();
}

void UShotgunBallisticsComponent::ShootPellet(const FVector& Location, const FVector& Direction)
{
	// Pellet line trace (reuses base class logic pattern)
	FVector End = Location + (Direction * MaxTraceDistance);

	FCollisionQueryParams TraceParams;
	TraceParams.bTraceComplex = true;
	TraceParams.bReturnPhysicalMaterial = true;
	TraceParams.AddIgnoredActor(GetOwner());
	if (GetOwner()->GetOwner())
	{
		TraceParams.AddIgnoredActor(GetOwner()->GetOwner());
	}

	TArray<FHitResult> HitResults;
	bool bHit = GetWorld()->LineTraceMultiByChannel(
		HitResults,
		Location,
		End,
		ECC_GameTraceChannel2,
		TraceParams
	);

	if (HitResults.Num() == 0)
	{
		return;
	}

	// Initialize pellet ballistic state from ammo type
	float Speed = CurrentAmmoType->MuzzleVelocity;
	float Mass = CurrentAmmoType->ProjectileMass;
	float Penetration = CurrentAmmoType->PenetrationPower;
	float DropFactor = CurrentAmmoType->DragCoefficient;
	float KineticEnergy = 0.0f;
	FVector LastHitLocation = Location;

	// Process hits using base class protected method
	for (int32 HitIndex = 0; HitIndex < HitResults.Num(); HitIndex++)
	{
		const FHitResult& Hit = HitResults[HitIndex];
		bool bContinue = ProcessHit(
			Hit,
			HitIndex,
			Speed,
			Mass,
			Penetration,
			DropFactor,
			KineticEnergy,
			Direction,
			LastHitLocation
		);

		if (!bContinue)
		{
			break;
		}

		LastHitLocation = Hit.ImpactPoint;
	}
}
