// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/ItemDataAsset.h"
#include "HoldableItemDataAsset.generated.h"

/**
 * Data asset for items that can be held in hands
 * Contains visual data: meshes, offsets, animations, camera settings
 *
 * Replaces ALL getters from old IHoldableItemInterface
 * Design: ONE data reference instead of 15+ getter functions
 */
UCLASS(BlueprintType)
class FPSCORE_API UHoldableItemDataAsset : public UItemDataAsset
{
	GENERATED_BODY()

public:
	// ============================================
	// MESHES
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Mesh")
	TSoftObjectPtr<USkeletalMesh> FPSMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Mesh")
	TSoftObjectPtr<USkeletalMesh> TPSMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Mesh")
	FRotator TPSRotationOffset = FRotator::ZeroRotator;

	// ============================================
	// HAND OFFSETS (IK)
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Hands")
	FVector HandsOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Hands")
	FVector AimHandsOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Hands")
	FVector RightHandOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Hands")
	FRotator RightHandRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Hands")
	bool bUseLeftHandIK = true;

	// ============================================
	// ANIMATION
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimationLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* UnequipMontage;

	// ============================================
	// CAMERA/AIMING
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	float AimFOV = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	float AimLookSpeed = 0.5f;

	// ============================================
	// AUDIO
	// ============================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	USoundBase* EquipSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	USoundBase* UnequipSound;
};
