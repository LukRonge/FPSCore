// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/HoldableItemDataAsset.h"
#include "ThrowableDataAsset.generated.h"

/**
 * Data asset for throwable items (grenades, flashbangs, etc.)
 * Contains throw physics, explosion stats, charges
 */
UCLASS(BlueprintType)
class FPSCORE_API UThrowableDataAsset : public UHoldableItemDataAsset
{
	GENERATED_BODY()

public:
	// ============================================
	// THROW PHYSICS
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Physics")
	float ThrowForce = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Physics")
	float ThrowAngle = 45.0f;  // degrees

	// ============================================
	// CHARGES
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Charges")
	int32 DefaultCharges = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Charges")
	int32 MaxCharges = 3;

	// ============================================
	// EXPLOSION
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Explosion")
	float FuseTime = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Explosion")
	float ExplosionRadius = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Explosion")
	float ExplosionDamage = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Explosion")
	TSubclassOf<UDamageType> DamageType;

	// ============================================
	// VFX/AUDIO
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Effects")
	UParticleSystem* ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Effects")
	USoundBase* ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Effects")
	TSubclassOf<UCameraShakeBase> ExplosionCameraShake;

	// ============================================
	// ANIMATION
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Animation")
	UAnimMontage* ThrowMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Animation")
	UAnimMontage* PullPinMontage;
};
