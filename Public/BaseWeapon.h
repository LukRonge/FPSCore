// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundCue.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/AmmoCaliberTypes.h"
#include "BaseWeapon.generated.h"

class ABaseMagazine;

UCLASS()
class FPSCORE_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABaseWeapon();

protected:
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// COMPONENTS
	// ============================================

	// First-person mesh (visible only to owner)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	USkeletalMeshComponent* FPSMesh;

	// Third-person mesh (visible to everyone except owner)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	USkeletalMeshComponent* TPSMesh;

	// ============================================
	// ITEM INFO
	// ============================================

	// Weapon display name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Info")
	FText Name;

	// Weapon description
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Info", meta = (MultiLine = true))
	FText Description;

	// Additional item information
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Info", meta = (MultiLine = true))
	FText ItemInfo;

	// ============================================
	// ANIMATION
	// ============================================

	// Animation layer class for weapon-specific animations
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> AnimLayer;

	// Shooting animation montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* ShootMontage;

	// Reload animation montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* ReloadMontage;

	// ============================================
	// HANDS / IK
	// ============================================

	// Hand offset for weapon holding (location correction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hands")
	FVector HandsOffset = FVector::ZeroVector;

	// Hand offset when aiming (location correction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hands")
	FVector AimHandsOffset = FVector::ZeroVector;

	// ============================================
	// SHOOTING
	// ============================================

	// Fire rate in rounds per minute (RPM)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float FireRate = 600.0f;

	// Weapon spread/accuracy (0 = perfect accuracy, higher = more spread)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float Spread = 0.5f;

	// Recoil intensity multiplier
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float RecoilScale = 1.0f;

	// ============================================
	// BREATHING / SWAY
	// ============================================

	// Breathing sway intensity (idle)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Breathing")
	float BreathingScale = 1.0f;

	// Breathing sway intensity when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Breathing")
	float AimBreathingScale = 0.3f;

	// ============================================
	// AIMING
	// ============================================

	// Field of view when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	float AimFOV = 50.0f;

	// Look sensitivity multiplier when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	float AimLookSpeed = 0.5f;

	// Is player currently aiming (runtime state, NOT replicated)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Aiming")
	bool IsAiming = false;

	// ============================================
	// UI
	// ============================================

	// Crosshair widget class (hip fire)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> CrossHair;

	// Crosshair widget class (aiming down sights)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> AimCrossHair;

	// ============================================
	// EFFECTS
	// ============================================

	// Muzzle flash Niagara system
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	UNiagaraSystem* MuzzleFlashNiagara;

	// ============================================
	// STATE
	// ============================================

	// Is weapon currently reloading (runtime state, NOT replicated)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|State")
	bool IsReload = false;

	// ============================================
	// AMMO SYSTEM
	// ============================================

	// Accepted caliber type (for ammo compatibility checks)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Ammo")
	EAmmoCaliberType AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

	// ============================================
	// MAGAZINE SYSTEM
	// ============================================

	// Current attached magazine reference (REPLICATED)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Magazine", Replicated)
	ABaseMagazine* CurrentMagazine = nullptr;

	// Magazine class for spawning/initialization
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Magazine")
	TSubclassOf<ABaseMagazine> MagazineClass;

	/**
	 * Check if this weapon can accept specific ammo type
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool CanAcceptAmmoType(EAmmoCaliberType CaliberType) const
	{
		return AcceptedCaliberType == CaliberType;
	}
};
