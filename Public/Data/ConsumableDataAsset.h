// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/HoldableItemDataAsset.h"
#include "ConsumableDataAsset.generated.h"

/**
 * Data asset for consumable items (medkits, food, etc.)
 * Contains use duration, healing/buff stats, charges
 */
UCLASS(BlueprintType)
class FPSCORE_API UConsumableDataAsset : public UHoldableItemDataAsset
{
	GENERATED_BODY()

public:
	// ============================================
	// CHARGES
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Charges")
	int32 DefaultCharges = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Charges")
	int32 MaxCharges = 1;

	// ============================================
	// USAGE
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Usage")
	float UseDuration = 2.0f;  // Channeling time in seconds

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Usage")
	bool bCanMoveWhileUsing = false;

	// ============================================
	// EFFECTS
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	float HealAmount = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	float HealOverTime = 0.0f;  // HP per second during channel

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	float StaminaRestore = 0.0f;

	// ============================================
	// ANIMATION
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Animation")
	UAnimMontage* UseMontage;

	// ============================================
	// VFX/AUDIO
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	UParticleSystem* UseEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	USoundBase* UseSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effects")
	USoundBase* ConsumeSound;
};
