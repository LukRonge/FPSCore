// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/HoldableItemDataAsset.h"
#include "Core/AmmoCaliberTypes.h"
#include "WeaponDataAsset.generated.h"

/**
 * Data asset for firearms
 * Contains weapon stats, ammo, recoil, crosshair, magazine visual
 *
 * Replaces ALL getters from old IWeaponInterface
 */
UCLASS(BlueprintType)
class FPSCORE_API UWeaponDataAsset : public UHoldableItemDataAsset
{
	GENERATED_BODY()

public:
	// ============================================
	// AMMO
	// ============================================

	// Accepted caliber type for quick compatibility check (e.g. pickup ammo from ground)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	EAmmoCaliberType AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MagazineCapacity = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxTotalAmmo = 210;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 DefaultAmmoInClip = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 DefaultTotalAmmo = 90;

	// ============================================
	// SHOOTING
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float FireRate = 600.0f;  // Rounds per minute

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float Damage = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float Range = 50000.0f;  // cm

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	bool bAutomatic = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	int32 BulletsPerShot = 1;  // For shotguns

	// ============================================
	// RECOIL
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	FVector2D RecoilPattern = FVector2D(0.5f, 1.0f);  // X=horizontal, Y=vertical

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilRecoverySpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float BreathingScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float AimBreathingScale = 0.3f;

	// ============================================
	// CROSSHAIR
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> CrosshairWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> AimCrosshairWidget;

	// ============================================
	// MAGAZINE VISUAL
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Magazine")
	TSoftObjectPtr<UStaticMesh> MagazineMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Magazine")
	FTransform MagazineLeftHandTransform;

	// ============================================
	// RELOAD
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Reload")
	UAnimMontage* ReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Reload")
	float ReloadDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Reload")
	USoundBase* ReloadSound;

	// ============================================
	// VFX/AUDIO
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	UParticleSystem* MuzzleFlash;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	USoundBase* FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	TSubclassOf<UCameraShakeBase> FireCameraShake;
};
