// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/FireComponent.h"
#include "FullAutoFireComponent.generated.h"

/**
 * Full-Automatic Fire Component
 * Continuous fire while trigger held, rate limited by FireRate
 *
 * FIRE MODE: Full-Auto
 *
 * BEHAVIOR:
 * - TriggerPulled() → Fire first shot immediately, start timer for continuous fire
 * - TriggerReleased() → Stop timer, cease fire
 * - Fire rate limited by FireRate property (RPM)
 * - Automatically stops when out of ammo
 *
 * USE CASES:
 * - Assault rifles (AK-47, M4A1 auto mode, SCAR)
 * - SMGs (MP5, UZI, P90)
 * - LMGs (M249, PKM, MG42)
 *
 * EXAMPLE:
 * AK-47 (600 RPM): Player holds trigger → 10 shots/second → Release → Stop
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UFullAutoFireComponent : public UFireComponent
{
	GENERATED_BODY()

public:
	// ============================================
	// FIRE CONTROL OVERRIDE
	// ============================================

	/**
	 * Trigger pulled - fire first shot and start continuous fire timer
	 */
	virtual void TriggerPulled() override;

	/**
	 * Trigger released - stop continuous fire timer
	 */
	virtual void TriggerReleased() override;

protected:
	/**
	 * Fire override - includes auto-stop when out of ammo
	 */
	virtual void Fire() override;
};
