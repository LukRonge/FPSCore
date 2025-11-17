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
	 * Get aiming crosshair widget class (shown when aiming down sights)
	 * Returns nullptr if no custom aim crosshair (uses weapon default)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	TSubclassOf<UUserWidget> GetAimingCrosshair() const;
	virtual TSubclassOf<UUserWidget> GetAimingCrosshair_Implementation() const { return nullptr; }

	/**
	 * Get aiming point offset relative to weapon mesh (where eye looks when aiming)
	 * Used to position camera when ADS (Aim Down Sights)
	 * Typically aligned with rear sight bone position
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight")
	FVector GetAimingPoint() const;
	virtual FVector GetAimingPoint_Implementation() const { return FVector::ZeroVector; }

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
};
