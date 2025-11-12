// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BallisticsComponent.h"
#include "Data/AmmoTypeDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "DrawDebugHelpers.h"

UBallisticsComponent::UBallisticsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// NOT replicated - this is a pure utility component
	SetIsReplicatedByDefault(false);

	// Initialize current ammo type
	CurrentAmmoType = nullptr;

	// Initialize CaliberDataMap with default ammo types
	TSoftObjectPtr<UAmmoTypeDataAsset> AmmoType_556NATO(FSoftObjectPath(TEXT("/Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/AmmoTypes/AmmoType_5_56x45mm_NATO.AmmoType_5_56x45mm_NATO'")));
	CaliberDataMap.Add(EAmmoCaliberType::NATO_556x45mm, AmmoType_556NATO);
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

	// Validate CurrentAmmoType
	if (!CurrentAmmoType)
	{
		UE_LOG(LogTemp, Error, TEXT("BallisticsComponent::Shoot() - No CurrentAmmoType set! Call InitAmmoType() first."));
		return;
	}

	// Log shoot event
	UE_LOG(LogTemp, Log, TEXT("BallisticsComponent::Shoot() - Location: %s, Direction: %s, Caliber: %s"),
		*Location.ToString(),
		*Direction.ToString(),
		*CurrentAmmoType->AmmoName.ToString());

	// ============================================
	// STEP 1: LINE TRACE SETUP
	// ============================================

	// Calculate end point
	FVector End = Location + (Direction * MaxTraceDistance);

	// Setup trace parameters
	FCollisionQueryParams TraceParams;
	TraceParams.bTraceComplex = true; // ✅ Use complex collision to get Physical Material
	TraceParams.bReturnPhysicalMaterial = true; // ✅ Explicitly request Physical Material
	TraceParams.AddIgnoredActor(GetOwner()); // Ignore weapon/character owner
	if (GetOwner()->GetOwner())
	{
		TraceParams.AddIgnoredActor(GetOwner()->GetOwner()); // Ignore character holding weapon
	}

	// Perform multi-line trace
	TArray<FHitResult> HitResults;
	bool bHit = GetWorld()->LineTraceMultiByChannel(
		HitResults,
		Location,
		End,
		ECC_GameTraceChannel2, // Projectile channel
		TraceParams
	);

	if (!bHit || HitResults.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BallisticsComponent::Shoot() - No hits detected"));

		// Display miss message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Green,
				TEXT("❌ MISS - No hit detected")
			);
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BallisticsComponent::Shoot() - Hit %d objects"), HitResults.Num());

	// Display hit count on screen
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Orange,
			FString::Printf(TEXT("🎯 HIT %d object(s)"), HitResults.Num())
		);
	}

	// ============================================
	// STEP 2: PROCESS HIT RESULTS
	// ============================================

	// Initialize ballistic variables from CurrentAmmoType
	float Speed = CurrentAmmoType->MuzzleVelocity;
	float Mass = CurrentAmmoType->ProjectileMass;
	float Penetration = CurrentAmmoType->PenetrationPower;
	float DropFactor = CurrentAmmoType->DragCoefficient;
	float KineticEnergy = 0.0f;

	// Track last hit location for distance calculation
	FVector LastHitLocation = Location; // Start from muzzle location

	// Process each hit
	for (int32 HitIndex = 0; HitIndex < HitResults.Num(); HitIndex++)
	{
		const FHitResult& Hit = HitResults[HitIndex];

		// Process hit (returns false if bullet stopped)
		bool bContinue = ProcessHit(Hit, HitIndex, Speed, Mass, Penetration, DropFactor, KineticEnergy, Direction, LastHitLocation);

		if (!bContinue)
		{
			UE_LOG(LogTemp, Log, TEXT("BallisticsComponent::Shoot() - Bullet stopped at hit %d"), HitIndex);
			break;
		}

		// Update last hit location for next iteration
		LastHitLocation = Hit.ImpactPoint;
	}
}

// ============================================
// HIT PROCESSING
// ============================================

bool UBallisticsComponent::ProcessHit(
	const FHitResult& Hit,
	int32 HitIndex,
	float& Speed,
	float& Mass,
	float& Penetration,
	float DropFactor,
	float& KineticEnergy,
	const FVector& Direction,
	const FVector& LastHitLocation)
{
	// ============================================
	// Extract hit data
	// ============================================
	AActor* HitActor = Hit.GetActor();
	FVector ImpactPoint = Hit.ImpactPoint;
	FVector ImpactNormal = Hit.ImpactNormal;
	UPhysicalMaterial* PhysMaterial = Hit.PhysMaterial.Get();
	FName BoneName = Hit.BoneName;

	if (!HitActor)
	{
		return true; // Skip invalid hits but continue
	}

	// ============================================
	// FIRST HIT: Initialize kinetic energy
	// ============================================
	if (HitIndex == 0)
	{
		// Calculate initial kinetic energy: KE = (m * v²) / 2
		// Simplified: (mass_grams / 1000) * (speed_m/s² / 1000)
		KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);

		UE_LOG(LogTemp, Log, TEXT("ProcessHit() - First Hit: Speed=%.1f m/s, Mass=%.2f g, Penetration=%.3f, KE=%.1f J"),
			Speed, Mass, Penetration, KineticEnergy);
	}

	// ============================================
	// DISTANCE DECAY: Apply air resistance
	// ============================================

	// Calculate distance traveled since last hit (or from muzzle if first hit)
	float DistanceTraveled = (ImpactPoint - LastHitLocation).Size();

	// Apply distance decay (air resistance, drag)
	ApplyDistanceDecay(Speed, Mass, DropFactor, KineticEnergy, DistanceTraveled);

	UE_LOG(LogTemp, Log, TEXT("ProcessHit() - Hit[%d]: Actor=%s, Location=%s, KE=%.1f J (AFTER distance decay, BEFORE penetration)"),
		HitIndex, *HitActor->GetName(), *ImpactPoint.ToString(), KineticEnergy);

	// ============================================
	// DEBUG VISUALIZATION: Impact Sphere (scaled by KE)
	// ============================================

	// Calculate sphere radius based on kinetic energy
	// Initial KE (~3.5 J) → 30 cm, Final KE (~0.3 J) → 5 cm
	float SphereRadius = FMath::Clamp(KineticEnergy * 10.0f, 5.0f, 30.0f);

	// Color gradient based on KE
	FColor SphereColor = KineticEnergy >= 2.0f ? FColor::Red : (KineticEnergy >= 1.0f ? FColor::Orange : FColor::Yellow);

	DrawDebugSphere(
		GetWorld(),
		ImpactPoint,
		SphereRadius,
		12, // Segments
		SphereColor,
		false,
		3.0f, // Duration
		0,
		2.0f // Thickness
	);

	// ============================================
	// STEP 3: Broadcast Impact Event (for visual effects)
	// ============================================

	// Get material name for VFX lookup
	FName MaterialName;
	bool bIsThin = IsThinMaterial(PhysMaterial, MaterialName);

	// Display physics material on screen
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			FString::Printf(TEXT("💥 HIT[%d]: %s %s"),
				HitIndex,
				*MaterialName.ToString(),
				bIsThin ? TEXT("(THIN)") : TEXT("(SOLID)"))
		);
	}

	// Lookup VFX from CurrentAmmoType on server
	UNiagaraSystem* ImpactVFX = CurrentAmmoType->GetImpactVFX(MaterialName);

	if (ImpactVFX && OnImpactDetected.IsBound())
	{
		// Broadcast event to weapon (SERVER ONLY)
		// Weapon will call Multicast RPC to spawn effects on all clients
		OnImpactDetected.Broadcast(
			ImpactVFX,
			FVector_NetQuantize(ImpactPoint),
			FVector_NetQuantizeNormal(ImpactNormal)
		);

		UE_LOG(LogTemp, Log, TEXT("ProcessHit() - Broadcasted impact event: Material=%s, VFX=%s"),
			*MaterialName.ToString(), *ImpactVFX->GetName());
	}

	// ============================================
	// STEP 4: Apply Damage
	// ============================================

	// Calculate damage scaled by kinetic energy
	// BaseDamage from CurrentAmmoType, scaled by remaining kinetic energy
	float FinalDamage = CurrentAmmoType->Damage * KineticEnergy;

	// Apply point damage to hit actor
	if (FinalDamage > 0.0f)
	{
		UGameplayStatics::ApplyPointDamage(
			HitActor,
			FinalDamage,
			Direction,
			Hit,
			GetOwner()->GetInstigatorController(),
			GetOwner(),
			UDamageType::StaticClass()
		);

		UE_LOG(LogTemp, Log, TEXT("ProcessHit() - Applied Damage: %.1f (BaseDamage: %.1f * KE: %.1f) to %s"),
			FinalDamage, CurrentAmmoType->Damage, KineticEnergy, *HitActor->GetName());
	}

	// ============================================
	// STEP 5: Apply Physics Impulse
	// ============================================

	// Get hit component (mesh/primitive component that was hit)
	UPrimitiveComponent* HitComponent = Hit.GetComponent();

	if (HitComponent && HitComponent->IsSimulatingPhysics(BoneName))
	{
		// Calculate impulse: Direction * (Mass * Speed * 0.01)
		// Scale factor 0.01 converts realistic physics to game-feel physics
		FVector Impulse = Direction * (Mass * Speed * 0.01f);

		// Apply impulse at impact point on specific bone
		HitComponent->AddImpulseAtLocation(
			Impulse,
			ImpactPoint,
			BoneName
		);

		UE_LOG(LogTemp, Log, TEXT("ProcessHit() - Applied Impulse: %s (Magnitude: %.1f) to %s at bone %s"),
			*Impulse.ToString(), Impulse.Size(), *HitComponent->GetName(), *BoneName.ToString());
	}

	// ============================================
	// PENETRATION LOSS: Apply after damage/impulse
	// ============================================

	// Apply penetration loss (reduces KE, Mass, Speed for NEXT hit)
	bool bCanContinue = ApplyPenetrationLoss(Speed, Mass, Penetration, DropFactor, KineticEnergy, PhysMaterial);

	if (!bCanContinue)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProcessHit() - Bullet stopped after penetration loss at hit %d"), HitIndex);
		return false; // Bullet stopped - don't process further hits
	}

	return true; // Continue processing next hits
}

bool UBallisticsComponent::IsThinMaterial(UPhysicalMaterial* PhysMaterial, FName& OutMaterialName) const
{
	// Default material name if no PhysMaterial provided
	if (!PhysMaterial)
	{
		OutMaterialName = FName(TEXT("Default"));
		UE_LOG(LogTemp, Warning, TEXT("IsThinMaterial() - PhysMaterial is nullptr! Treating as SOLID 'Default' material."));
		return false; // Nullptr = treat as THIN to allow penetration
	}

	// Get material name from PhysicalMaterial
	OutMaterialName = PhysMaterial->GetFName();

	// Check if material name contains "Thin" substring
	FString MaterialNameStr = OutMaterialName.ToString();
	bool bIsThin = MaterialNameStr.Contains(TEXT("Thin"), ESearchCase::IgnoreCase);

	UE_LOG(LogTemp, Log, TEXT("IsThinMaterial() - PhysMaterial='%s', Contains 'Thin'=%s"),
		*MaterialNameStr, bIsThin ? TEXT("YES") : TEXT("NO"));

	return bIsThin;
}

void UBallisticsComponent::ApplyDistanceDecay(
	float& Speed,
	float& Mass,
	float DropFactor,
	float& KineticEnergy,
	float Distance)
{
	if (Distance <= 0.0f)
	{
		return; // No distance traveled, no decay
	}

	// Convert distance from cm to meters for realistic calculations
	float DistanceMeters = Distance / 100.0f;

	// Apply exponential velocity decay based on drag coefficient
	// Formula: v(d) = v0 * exp(-DragCoefficient * distance / 1000)
	// Divide by 1000 to scale for reasonable game distances
	float DecayFactor = FMath::Exp(-DropFactor * DistanceMeters / 1000.0f);
	Speed *= DecayFactor;

	// Recalculate kinetic energy after speed change
	// Mass stays the same (no fragmentation in air)
	KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);

	UE_LOG(LogTemp, Log, TEXT("ApplyDistanceDecay() - Distance: %.1f m, DecayFactor: %.3f, Speed: %.1f m/s, KE: %.1f J"),
		DistanceMeters, DecayFactor, Speed, KineticEnergy);
}

bool UBallisticsComponent::ApplyPenetrationLoss(
	float& Speed,
	float& Mass,
	float& Penetration,
	float DropFactor,
	float& KineticEnergy,
	UPhysicalMaterial* PhysMaterial)
{
	// Get material name and check if thin
	FName MaterialName;
	bool bIsThin = IsThinMaterial(PhysMaterial, MaterialName);

	// ============================================
	// SOLID MATERIALS: Block bullet immediately
	// ============================================
	if (!bIsThin)
	{
		// Solid material detected (includes nullptr PhysMaterial)
		// Bullet STOPS and does NOT continue to next hit
		UE_LOG(LogTemp, Warning, TEXT("ApplyPenetrationLoss() - SOLID MATERIAL '%s' blocks penetration! Bullet STOPPED."),
			*MaterialName.ToString());

		// Apply heavy energy loss and fragmentation (for damage calculation on current hit)
		Penetration *= (KineticEnergy * 0.5f);
		Mass *= 0.85f; // Projectile deformation/fragmentation
		Speed *= DropFactor;
		KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);

		return false; // Bullet stopped - do NOT continue
	}

	// ============================================
	// THIN MATERIALS: Allow penetration
	// ============================================

	// Thin materials: light penetration loss, no fragmentation
	Penetration *= (KineticEnergy * 0.5f);
	// Mass stays the same (no projectile deformation)

	// Apply velocity drop (exponential decay)
	Speed *= DropFactor;

	// Recalculate kinetic energy after velocity/mass changes
	KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);

	UE_LOG(LogTemp, Log, TEXT("ApplyPenetrationLoss() - THIN MATERIAL '%s' - Bullet continues. Speed=%.1f m/s, Mass=%.2f g, Penetration=%.3f, KE=%.1f J"),
		*MaterialName.ToString(),
		Speed, Mass, Penetration, KineticEnergy);

	// Check penetration threshold - bullet stopped due to energy loss
	if (Penetration <= 0.002f)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyPenetrationLoss() - Bullet stopped (Penetration <= 0.002)"));
		return false; // Bullet stopped
	}

	return true; // Bullet can continue through thin material
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
// CALIBER DATA LOOKUP
// ============================================

bool UBallisticsComponent::InitAmmoType(EAmmoCaliberType CaliberType)
{
	if (const TSoftObjectPtr<UAmmoTypeDataAsset>* FoundAsset = CaliberDataMap.Find(CaliberType))
	{
		CurrentAmmoType = FoundAsset->LoadSynchronous();

		if (CurrentAmmoType)
		{
			UE_LOG(LogTemp, Log, TEXT("BallisticsComponent::InitAmmoType() - Loaded caliber: %s"),
				*CurrentAmmoType->AmmoName.ToString());
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("BallisticsComponent::InitAmmoType() - Failed to load asset for caliber type '%s'"),
				*UEnum::GetValueAsString(CaliberType));
			return false;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BallisticsComponent::InitAmmoType() - Caliber type '%s' not found in CaliberDataMap!"),
		*UEnum::GetValueAsString(CaliberType));

	CurrentAmmoType = nullptr;
	return false;
}

UAmmoTypeDataAsset* UBallisticsComponent::GetAmmoDataForCaliber(EAmmoCaliberType CaliberType) const
{
	if (const TSoftObjectPtr<UAmmoTypeDataAsset>* FoundAsset = CaliberDataMap.Find(CaliberType))
	{
		return FoundAsset->LoadSynchronous();
	}

	UE_LOG(LogTemp, Warning, TEXT("BallisticsComponent::GetAmmoDataForCaliber() - Caliber type '%s' not found in CaliberDataMap!"),
		*UEnum::GetValueAsString(CaliberType));

	return nullptr;
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
