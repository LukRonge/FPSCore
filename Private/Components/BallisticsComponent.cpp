// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BallisticsComponent.h"
#include "Data/AmmoTypeDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "DrawDebugHelpers.h"
#include "Interfaces/BallisticsHandlerInterface.h"

UBallisticsComponent::UBallisticsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	CurrentAmmoType = nullptr;

	TSoftObjectPtr<UAmmoTypeDataAsset> AmmoType_556NATO(FSoftObjectPath(TEXT("/Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/Weapons/AmmoTypes/AmmoType_5_56x45mm_NATO.AmmoType_5_56x45mm_NATO'")));
	CaliberDataMap.Add(EAmmoCaliberType::NATO_556x45mm, AmmoType_556NATO);
}

void UBallisticsComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBallisticsComponent::Shoot(FVector Location, FVector Direction)
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("[BALLISTICS] Called on CLIENT! Projectiles spawn on server only."));
		return;
	}

	if (!CurrentAmmoType)
	{
		UE_LOG(LogTemp, Error, TEXT("[BALLISTICS] No CurrentAmmoType set! Call InitAmmoType() first."));
		return;
	}

	// ✅ Notify owner via IBallisticsHandlerInterface
	// This allows BaseWeapon to spawn muzzle flash immediately
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->Implements<UBallisticsHandlerInterface>())
	{
		IBallisticsHandlerInterface::Execute_HandleShotFired(
			OwnerActor,
			FVector_NetQuantize(Location),
			FVector_NetQuantizeNormal(Direction)
		);
	}

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

	// Process OVERLAP hits even if bHit=false (no BLOCK collision)
	if (HitResults.Num() == 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[BALLISTICS] No hits detected - trace distance: %.2f meters"), (End - Location).Size() / 100.0f);
		return;
	}

	float Speed = CurrentAmmoType->MuzzleVelocity;
	float Mass = CurrentAmmoType->ProjectileMass;
	float Penetration = CurrentAmmoType->PenetrationPower;
	float DropFactor = CurrentAmmoType->DragCoefficient;
	float KineticEnergy = 0.0f;
	FVector LastHitLocation = Location;

	for (int32 HitIndex = 0; HitIndex < HitResults.Num(); HitIndex++)
	{
		const FHitResult& Hit = HitResults[HitIndex];
		bool bContinue = ProcessHit(Hit, HitIndex, Speed, Mass, Penetration, DropFactor, KineticEnergy, Direction, LastHitLocation);

		if (!bContinue)
		{
			UE_LOG(LogTemp, Warning, TEXT("[KE] Bullet STOPPED at hit %d/%d"), HitIndex, HitResults.Num());
			break;
		}

		LastHitLocation = Hit.ImpactPoint;
	}

	UE_LOG(LogTemp, Warning, TEXT("[KE] Processed %d hits total"), HitResults.Num());
}

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
	AActor* HitActor = Hit.GetActor();
	FVector ImpactPoint = Hit.ImpactPoint;
	FVector ImpactNormal = Hit.ImpactNormal;
	UPhysicalMaterial* PhysMaterial = Hit.PhysMaterial.Get();
	FName BoneName = Hit.BoneName;

	if (!HitActor)
	{
		return true;
	}

	// Initialize KE on first hit
	if (HitIndex == 0)
	{
		// KE = 0.5 * m * v² (physics formula)
		// Mass in kg (g / 1000), Speed in m/s
		KineticEnergy = 0.5f * (Mass / 1000.0f) * Speed * Speed;
		UE_LOG(LogTemp, Warning, TEXT("[KE] INITIAL - Speed=%.1f m/s, Mass=%.2f g, KE=%.1f J"),
			Speed, Mass, KineticEnergy);
	}

	// Distance decay
	float DistanceTraveled = (ImpactPoint - LastHitLocation).Size();
	ApplyDistanceDecay(Speed, Mass, DropFactor, KineticEnergy, DistanceTraveled);

	// Notify owner via IBallisticsHandlerInterface for impact VFX
	FName MaterialName;
	bool bIsThin = IsThinMaterial(PhysMaterial, MaterialName);
	UNiagaraSystem* ImpactVFX = CurrentAmmoType->GetImpactVFX(MaterialName);

	if (ImpactVFX)
	{
		AActor* OwnerActor = GetOwner();
		if (OwnerActor && OwnerActor->Implements<UBallisticsHandlerInterface>())
		{
			IBallisticsHandlerInterface::Execute_HandleImpactDetected(
				OwnerActor,
				ImpactVFX,
				FVector_NetQuantize(ImpactPoint),
				FVector_NetQuantizeNormal(ImpactNormal)
			);
		}
	}

	// Apply damage
	// Normalize KE against reference value (5.56mm NATO at muzzle = ~1500 J)
	// This allows Damage parameter to stay in reasonable range (20-100)
	// while preserving distance/penetration decay effects
	const float ReferenceKE = 1500.0f;
	float KE_Multiplier = KineticEnergy / ReferenceKE;
	float FinalDamage = CurrentAmmoType->Damage * KE_Multiplier;

	// DEBUG: Check damage calculation
	if (GEngine)
	{
		FString DebugMsg = FString::Printf(TEXT("[BALLISTICS] KE=%.1f J | BaseDamage=%.1f | Multiplier=%.3f | FinalDamage=%.1f"),
			KineticEnergy, CurrentAmmoType->Damage, KE_Multiplier, FinalDamage);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
	}

	if (FinalDamage > 0.0f)
	{
		AActor* Weapon = GetOwner();  // BaseWeapon
		AActor* Character = Weapon ? Weapon->GetOwner() : nullptr;  // FPSCharacter (weapon's owner)

		UGameplayStatics::ApplyPointDamage(
			HitActor,
			FinalDamage,
			Direction,
			Hit,
			Weapon ? Weapon->GetInstigatorController() : nullptr,
			Character,  // DamageCauser = FPSCharacter who fired the weapon
			UDamageType::StaticClass()
		);
	}

	// Apply physics impulse (based on momentum and kinetic energy)
	UPrimitiveComponent* HitComponent = Hit.GetComponent();

	if (HitComponent && HitComponent->IsSimulatingPhysics(BoneName))
	{
		// Calculate impulse from momentum: p = m × v
		// Mass in kg (grams / 1000), Speed in m/s, convert to cm/s for Unreal (*100)
		float MassKg = Mass / 1000.0f;
		float SpeedCmPerSec = Speed * 100.0f;
		float Momentum = MassKg * SpeedCmPerSec;  // kg⋅cm/s

		// Scale impulse by kinetic energy ratio (higher KE = stronger impact)
		// ReferenceKE already declared above for damage calculation (1500 J)
		float KE_ImpulseMultiplier = FMath::Sqrt(KineticEnergy / ReferenceKE);

		// Final impulse magnitude (momentum scaled by KE)
		float ImpulseMagnitude = Momentum * KE_ImpulseMultiplier;

		// Apply impulse in shot direction
		FVector Impulse = Direction * ImpulseMagnitude;

		HitComponent->AddImpulseAtLocation(Impulse, ImpactPoint, BoneName);

		// DEBUG: Log impulse application
		UE_LOG(LogTemp, Verbose, TEXT("[IMPULSE] Mass=%.2fg, Speed=%.1fm/s, KE=%.1fJ, Momentum=%.1f, Multiplier=%.2f, Final=%.1f"),
			Mass, Speed, KineticEnergy, Momentum, KE_ImpulseMultiplier, ImpulseMagnitude);
	}

	// Penetration loss
	bool bCanContinue = ApplyPenetrationLoss(Speed, Mass, Penetration, DropFactor, KineticEnergy, PhysMaterial);
	return bCanContinue;
}

bool UBallisticsComponent::IsThinMaterial(UPhysicalMaterial* PhysMaterial, FName& OutMaterialName) const
{
	if (!PhysMaterial)
	{
		OutMaterialName = FName(TEXT("Default"));
		return false;
	}

	OutMaterialName = PhysMaterial->GetFName();
	FString MaterialNameStr = OutMaterialName.ToString();
	return MaterialNameStr.Contains(TEXT("Thin"), ESearchCase::IgnoreCase);
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
		return;
	}

	float DistanceMeters = Distance / 100.0f;
	float DecayFactor = FMath::Exp(-DropFactor * DistanceMeters / 1000.0f);
	Speed *= DecayFactor;
	// Recalculate KE after speed decay
	KineticEnergy = 0.5f * (Mass / 1000.0f) * Speed * Speed;
}

bool UBallisticsComponent::ApplyPenetrationLoss(
	float& Speed,
	float& Mass,
	float& Penetration,
	float DropFactor,
	float& KineticEnergy,
	UPhysicalMaterial* PhysMaterial)
{
	FName MaterialName;
	bool bIsThin = IsThinMaterial(PhysMaterial, MaterialName);

	// SOLID: Block bullet
	if (!bIsThin)
	{
		Penetration *= (KineticEnergy * 0.5f);
		Mass *= 0.85f;
		Speed *= DropFactor;
		// Recalculate KE after penetration loss
		KineticEnergy = 0.5f * (Mass / 1000.0f) * Speed * Speed;
		return false;
	}

	// THIN: Allow penetration with energy loss
	float KEBefore = KineticEnergy;
	float SpeedBefore = Speed;

	const float ThinMaterialVelocityRetention = 0.65f;
	const float ThinMaterialPenetrationDecay = 0.5f;

	Speed *= ThinMaterialVelocityRetention;
	Penetration *= ThinMaterialPenetrationDecay;
	// Recalculate KE after thin material penetration
	KineticEnergy = 0.5f * (Mass / 1000.0f) * Speed * Speed;

	float KELossPercent = ((KEBefore - KineticEnergy) / KEBefore) * 100.0f;

	UE_LOG(LogTemp, Warning, TEXT("[KE] THIN '%s' - BEFORE: Speed=%.1f m/s, KE=%.3f J | AFTER: Speed=%.1f m/s, KE=%.3f J | Loss: %.1f%% | Penetration=%.4f"),
		*MaterialName.ToString(), SpeedBefore, KEBefore, Speed, KineticEnergy, KELossPercent, Penetration);

	if (Penetration <= 0.05f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[KE] STOPPED - Penetration exhausted (%.4f <= 0.05)"), Penetration);
		return false;
	}

	if (KineticEnergy <= 0.3f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[KE] STOPPED - Kinetic energy too low (%.3f J <= 0.3 J)"), KineticEnergy);
		return false;
	}

	return true;
}

float UBallisticsComponent::CalculateBulletDrop(float Distance) const
{
	UAmmoTypeDataAsset* CaliberData = CaliberDataAsset.LoadSynchronous();
	if (!CaliberData)
	{
		return 0.0f;
	}

	const float Gravity = 980.0f;
	const float Velocity = CaliberData->MuzzleVelocity * 100.0f;
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

	float Mass = CaliberData->ProjectileMass / 1000.0f;
	float Velocity = CaliberData->MuzzleVelocity;

	return 0.5f * Mass * Velocity * Velocity;
}

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
