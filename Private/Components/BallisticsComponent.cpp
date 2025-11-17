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
		KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);
		UE_LOG(LogTemp, Warning, TEXT("[KE] INITIAL - Speed=%.1f m/s, Mass=%.2f g, KE=%.3f J"),
			Speed, Mass, KineticEnergy);
	}

	// Distance decay
	float DistanceTraveled = (ImpactPoint - LastHitLocation).Size();
	ApplyDistanceDecay(Speed, Mass, DropFactor, KineticEnergy, DistanceTraveled);

	// Debug sphere
	float SphereRadius = FMath::Clamp(KineticEnergy * 10.0f, 5.0f, 30.0f);
	FColor SphereColor = KineticEnergy >= 2.0f ? FColor::Red : (KineticEnergy >= 1.0f ? FColor::Orange : FColor::Yellow);

	UE_LOG(LogTemp, Warning, TEXT("[KE] Hit[%d] - Actor=%s, KE=%.3f J, Radius=%.1f cm, Color=%s"),
		HitIndex, *HitActor->GetName(), KineticEnergy, SphereRadius,
		KineticEnergy >= 2.0f ? TEXT("RED") : (KineticEnergy >= 1.0f ? TEXT("ORANGE") : TEXT("YELLOW")));

	DrawDebugSphere(GetWorld(), ImpactPoint, SphereRadius, 12, SphereColor, false, 3.0f, 0, 2.0f);

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
	float FinalDamage = CurrentAmmoType->Damage * KineticEnergy;
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
	}

	// Apply physics impulse
	UPrimitiveComponent* HitComponent = Hit.GetComponent();

	if (HitComponent && HitComponent->IsSimulatingPhysics(BoneName))
	{
		FVector Impulse = Direction * (Mass * Speed * 0.01f);
		HitComponent->AddImpulseAtLocation(Impulse, ImpactPoint, BoneName);
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
	KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);
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
		KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);
		return false;
	}

	// THIN: Allow penetration with energy loss
	float KEBefore = KineticEnergy;
	float SpeedBefore = Speed;

	const float ThinMaterialVelocityRetention = 0.65f;
	const float ThinMaterialPenetrationDecay = 0.5f;

	Speed *= ThinMaterialVelocityRetention;
	Penetration *= ThinMaterialPenetrationDecay;
	KineticEnergy = (Mass / 1000.0f) * (Speed * Speed / 1000.0f);

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
