// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SightInterface.generated.h"

/**
 * CAPABILITY: Sightable
 * Interface for weapons/items that support aiming sights
 *
 * Implemented by: Weapons with iron sights, scopes, red dots
 * Design: Provides aiming configuration (crosshair, aiming point, hands offset)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class USightInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API ISightInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get hip-fire crosshair widget class (shown when not aiming)
	 * Returns nullptr if no crosshair
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	TSubclassOf<UUserWidget> GetCrossHair() const;
	virtual TSubclassOf<UUserWidget> GetCrossHair_Implementation() const { return nullptr; }

	/**
	 * Get aiming crosshair widget class (shown when aiming down sights)
	 * Returns nullptr if no custom aim crosshair (uses weapon default)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	TSubclassOf<UUserWidget> GetAimingCrosshair() const;
	virtual TSubclassOf<UUserWidget> GetAimingCrosshair_Implementation() const { return nullptr; }

	/**
	 * Get aim transform in WORLD SPACE (where camera aligns when aiming)
	 * Returns the world-space transform of the aiming point
	 *
	 * INVALID MARKER: If aiming is not possible, returns transform with Scale = ZeroVector
	 * Character should check: if (AimTransform.GetScale3D().IsNearlyZero()) → aiming not available
	 *
	 * Implementation:
	 * - BaseSight: Returns sight's AimingPoint socket/offset transformed to world space
	 * - BaseWeapon: Delegates to CurrentSight, or uses FPSMesh "aim" socket if no sight
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	FTransform GetAimTransform() const;
	virtual FTransform GetAimTransform_Implementation() const { return FTransform(FVector::ZeroVector); }

	/**
	 * Check if sight should hide first-person mesh when aiming
	 * Used for scopes/optics that obstruct view
	 * Returns true to hide FPS mesh during ADS
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	bool ShouldHideFPSMeshWhenAiming() const;
	virtual bool ShouldHideFPSMeshWhenAiming_Implementation() const { return false; }

	/**
	 * Get sight magnification (1.0 = no zoom, 2.0 = 2x, 4.0 = 4x, etc.)
	 * Used to calculate FOV when aiming: AimFOV = BaseFOV / Magnification
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	float GetSightMagnification() const;
	virtual float GetSightMagnification_Implementation() const { return 1.0f; }

	/**
	 * Get camera FOV when aiming down sights
	 * Returns 0.0 if not set (use default weapon AimFOV)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	float GetAimingFOV() const;
	virtual float GetAimingFOV_Implementation() const { return 0.0f; }

	/**
	 * Get look speed multiplier when aiming (0.5 = half speed, 1.0 = normal speed)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	float GetAimLookSpeed() const;
	virtual float GetAimLookSpeed_Implementation() const { return 1.0f; }

	/**
	 * Get leaning scale when aiming (0.0 = no lean, 1.0 = full lean)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	float GetAimLeaningScale() const;
	virtual float GetAimLeaningScale_Implementation() const { return 1.0f; }

	/**
	 * Get breathing scale when aiming (0.0 = no breathing, 1.0 = full breathing)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	float GetAimBreathingScale() const;
	virtual float GetAimBreathingScale_Implementation() const { return 0.3f; }
};
