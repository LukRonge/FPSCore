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

	// 5.56x45mm NATO - M4A1, M16, SCAR-L, HK416
	TSoftObjectPtr<UAmmoTypeDataAsset> AmmoType_556NATO(FSoftObjectPath(TEXT("/Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/Weapons/AmmoTypes/AmmoType_5_56x45mm_NATO.AmmoType_5_56x45mm_NATO'")));
	CaliberDataMap.Add(EAmmoCaliberType::NATO_556x45mm, AmmoType_556NATO);

	// 7.62x51mm NATO / .308 Win - Sako 85, G3, FAL, M14, SR-25
	TSoftObjectPtr<UAmmoTypeDataAsset> AmmoType_762NATO(FSoftObjectPath(TEXT("/Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/Weapons/AmmoTypes/AmmoType_7_62x51mm_NATO.AmmoType_7_62x51mm_NATO'")));
	CaliberDataMap.Add(EAmmoCaliberType::NATO_762x51mm, AmmoType_762NATO);

	// 9x19mm Parabellum - VP9, Glock, MP5, UZI
	TSoftObjectPtr<UAmmoTypeDataAsset> AmmoType_9mm(FSoftObjectPath(TEXT("/Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/Weapons/AmmoTypes/AmmoType_9mm.AmmoType_9mm'")));
	CaliberDataMap.Add(EAmmoCaliberType::Parabellum_9x19mm, AmmoType_9mm);
}

void UBallisticsComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBallisticsComponent::Shoot(FVector Location, FVector Direction)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!CurrentAmmoType)
	{
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

	if (HitResults.Num() == 0)
	{
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
			break;
		}

		LastHitLocation = Hit.ImpactPoint;
	}
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

	if (HitIndex == 0)
	{
		KineticEnergy = 0.5f * (Mass / 1000.0f) * Speed * Speed;
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

	const float ReferenceKE = 1500.0f;
	float KE_Multiplier = KineticEnergy / ReferenceKE;
	float FinalDamage = CurrentAmmoType->Damage * KE_Multiplier;

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

	UPrimitiveComponent* HitComponent = Hit.GetComponent();

	if (HitComponent && HitComponent->IsSimulatingPhysics(BoneName))
	{
		float MassKg = Mass / 1000.0f;
		float SpeedCmPerSec = Speed * 100.0f;
		float Momentum = MassKg * SpeedCmPerSec;
		float KE_ImpulseMultiplier = FMath::Sqrt(KineticEnergy / ReferenceKE);
		float ImpulseMagnitude = Momentum * KE_ImpulseMultiplier;
		FVector Impulse = Direction * ImpulseMagnitude;

		HitComponent->AddImpulseAtLocation(Impulse, ImpactPoint, BoneName);
	}

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

	const float ThinMaterialVelocityRetention = 0.65f;
	const float ThinMaterialPenetrationDecay = 0.5f;

	Speed *= ThinMaterialVelocityRetention;
	Penetration *= ThinMaterialPenetrationDecay;
	KineticEnergy = 0.5f * (Mass / 1000.0f) * Speed * Speed;

	if (Penetration <= 0.05f)
	{
		return false;
	}

	if (KineticEnergy <= 0.3f)
	{
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
		return CurrentAmmoType != nullptr;
	}

	CurrentAmmoType = nullptr;
	return false;
}

UAmmoTypeDataAsset* UBallisticsComponent::GetAmmoDataForCaliber(EAmmoCaliberType CaliberType) const
{
	if (const TSoftObjectPtr<UAmmoTypeDataAsset>* FoundAsset = CaliberDataMap.Find(CaliberType))
	{
		return FoundAsset->LoadSynchronous();
	}

	return nullptr;
}

void UBallisticsComponent::DebugPrintCaliberData() const
{
}
