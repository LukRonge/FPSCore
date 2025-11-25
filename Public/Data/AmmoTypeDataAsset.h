// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Core/AmmoCaliberTypes.h"
#include "AmmoTypeDataAsset.generated.h"

/**
 * Data asset for ammunition types
 * Contains ballistic properties, damage stats, and impact effects per material
 *
 * Used by weapons to define ammunition characteristics:
 * - Ballistics: mass, velocity, drop, penetration
 * - Damage: base damage, damage radius, falloff
 * - Impact VFX: different effects per physical material (concrete, metal, wood, etc.)
 *
 * AVAILABLE AMMO TYPES:
 * - 5.56x45mm NATO: /Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/Weapons/AmmoTypes/AmmoType_5_56x45mm_NATO.AmmoType_5_56x45mm_NATO'
 * - 9x19mm Parabellum: /Script/FPSCore.AmmoTypeDataAsset'/FPSCore/Blueprints/Weapons/AmmoTypes/AmmoType_9mm.AmmoType_9mm'
 *
 * RECOMMENDED VALUES FOR 9x19mm PARABELLUM:
 * - CaliberType: Parabellum_9x19mm
 * - ProjectileMass: 8.0g (124 grain FMJ)
 * - MuzzleVelocity: 375 m/s
 * - DragCoefficient: 0.165 (lower than rifle rounds)
 * - PenetrationPower: 0.35 (less penetration than rifle rounds)
 * - Damage: 25 (pistol caliber)
 */
UCLASS(BlueprintType)
class FPSCORE_API UAmmoTypeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ============================================
	// IDENTIFICATION
	// ============================================

	// Caliber type for quick matching (weapon compatibility check)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Info")
	EAmmoCaliberType CaliberType = EAmmoCaliberType::None;

	// Display name (e.g. "5.56x45mm NATO", "7.62x39mm")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Info")
	FText AmmoName;

	// Short identifier (e.g. "556", "762")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Info")
	FName AmmoID;

	// ============================================
	// BALLISTICS
	// ============================================

	// Projectile mass in grams (e.g. 4.0g for 5.56mm, 7.9g for 7.62x39mm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Ballistics", meta = (ClampMin = "0.1", ClampMax = "100.0"))
	float ProjectileMass = 4.0f;

	// Muzzle velocity in m/s (e.g. 940 m/s for 5.56mm, 715 m/s for 7.62x39mm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Ballistics", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float MuzzleVelocity = 900.0f;

	// Drag coefficient for bullet drop simulation (higher = more drop)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Ballistics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DragCoefficient = 0.295f;

	// Penetration power (affects material piercing, 0-1 scale)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Ballistics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PenetrationPower = 0.5f;

	// ============================================
	// VISUAL
	// ============================================

	// Shell casing mesh (ejected from weapon after firing)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Visual")
	TSoftObjectPtr<UStaticMesh> AmmoShell;

	// ============================================
	// DAMAGE
	// ============================================

	// Base damage per projectile at reference kinetic energy (1500 J = 5.56mm NATO muzzle)
	// Actual damage = Damage * (CurrentKE / ReferenceKE)
	// - Distance reduces KE → reduces damage
	// - Penetration reduces KE → reduces damage
	// Recommended values: 5.56mm=35, 7.62x39mm=45, .50 BMG=80
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Damage", meta = (ClampMin = "0.0", ClampMax = "200.0"))
	float Damage = 35.0f;

	// Damage radius for explosive rounds (0 = no splash damage)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Damage", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float DamageRadius = 1.0f;

	// ============================================
	// IMPACT EFFECTS
	// ============================================

	// Material-specific impact VFX (Key = Physical Material name: Concrete, Metal, Wood, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Effects")
	TMap<FName, TSoftObjectPtr<UNiagaraSystem>> ImpactVFXMap;

	// ============================================
	// HELPERS
	// ============================================

	/**
	 * Get impact VFX for specific physical material
	 * Falls back to default if material not found in map
	 */
	UFUNCTION(BlueprintCallable, Category = "Ammo")
	UNiagaraSystem* GetImpactVFX(FName PhysicalMaterialName) const
	{
		if (const TSoftObjectPtr<UNiagaraSystem>* FoundVFX = ImpactVFXMap.Find(PhysicalMaterialName))
		{
			return FoundVFX->LoadSynchronous();
		}
		return nullptr;
	}
};
