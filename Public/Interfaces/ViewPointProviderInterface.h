// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ViewPointProviderInterface.generated.h"

/**
 * CAPABILITY: ViewPointProvider
 * Interface for characters/pawns that provide aim view point and direction
 *
 * Implemented by: AFPSCharacter (custom Pitch handling)
 * Design: Provides server-authoritative view point for weapon firing
 *
 * Why needed:
 * - FPSCharacter has bUseControllerRotationPitch = false
 * - Pitch is stored as custom replicated property
 * - GetActorEyesViewPoint() returns Pitch=0 on server
 * - Weapons need correct Pitch for accurate ballistics
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UViewPointProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IViewPointProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get view point location and rotation for shooting
	 * Called by weapons on SERVER to get accurate aim direction
	 *
	 * @param OutLocation - Camera/eye location in world space
	 * @param OutRotation - Camera/eye rotation (includes custom Pitch)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ViewPoint")
	void GetShootingViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 * Get current camera pitch (replicated value)
	 * Used for weapons to calculate bullet trajectory
	 *
	 * @return Current pitch angle in degrees
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ViewPoint")
	float GetViewPitch() const;

	/**
	 * Get recoil accumulation factor for spread calculation
	 * Used by weapons to increase spread during sustained fire
	 *
	 * @return Recoil factor (0.0 = no recoil, 1.0 = maximum accumulation)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ViewPoint")
	float GetRecoilFactor() const;
	virtual float GetRecoilFactor_Implementation() const { return 0.0f; }
};
