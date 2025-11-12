// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BallisticsHandlerInterface.generated.h"

class UNiagaraSystem;

/**
 * CAPABILITY: BallisticsHandler
 * Interface for actors that handle ballistics events
 *
 * Implemented by: BaseWeapon (receives notifications from BallisticsComponent)
 * Called by: BallisticsComponent (notifies owner about shots/impacts)
 *
 * Design: BallisticsComponent calls these methods on owner when ballistic events occur
 * Owner can use this for: effects (VFX/audio), statistics, achievements, haptics, etc.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBallisticsHandlerInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IBallisticsHandlerInterface
{
	GENERATED_BODY()

public:
	/**
	 * Handle shot fired event (SERVER ONLY)
	 * Called by BallisticsComponent when shot is fired
	 *
	 * Typical usage: Spawn muzzle flash via Multicast RPC
	 *
	 * @param MuzzleLocation - Location where shot originated
	 * @param Direction - Direction of shot (normalized)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ballistics")
	void HandleShotFired(FVector_NetQuantize MuzzleLocation, FVector_NetQuantizeNormal Direction);
	virtual void HandleShotFired_Implementation(FVector_NetQuantize MuzzleLocation, FVector_NetQuantizeNormal Direction) {}

	/**
	 * Handle impact detected event (SERVER ONLY)
	 * Called by BallisticsComponent when projectile hits surface
	 *
	 * Typical usage: Spawn impact effects via Multicast RPC
	 *
	 * @param ImpactVFX - Niagara system to spawn (surface-specific from AmmoTypeDataAsset)
	 * @param Location - Impact location
	 * @param Normal - Impact surface normal
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ballistics")
	void HandleImpactDetected(const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX, FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal);
	virtual void HandleImpactDetected_Implementation(const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX, FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal) {}
};
