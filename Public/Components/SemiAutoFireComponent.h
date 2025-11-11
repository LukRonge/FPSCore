// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/FireComponent.h"
#include "SemiAutoFireComponent.generated.h"

/**
 * Semi-Automatic Fire Component
 * One shot per trigger pull, requires trigger release before next shot
 *
 * FIRE MODE: Semi-Auto
 *
 * BEHAVIOR:
 * - TriggerPulled() → Fire one shot immediately
 * - TriggerReleased() → Reset trigger state
 * - Subsequent TriggerPulled() calls ignored until trigger released
 * - No automatic fire rate limiting (manual fire only)
 *
 * USE CASES:
 * - Pistols (Glock, M1911, Desert Eagle)
 * - Semi-auto rifles (M14, SKS, DMR)
 * - Shotguns (Pump-action, Semi-auto)
 *
 * EXAMPLE:
 * M4A1 (Semi mode): Player clicks → 1 shot → Click again → 1 shot
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API USemiAutoFireComponent : public UFireComponent
{
	GENERATED_BODY()

public:
	// ============================================
	// FIRE CONTROL OVERRIDE
	// ============================================

	/**
	 * Trigger pulled - fire one shot if trigger not already held
	 */
	virtual void TriggerPulled() override;

	/**
	 * Trigger released - allow next shot
	 */
	virtual void TriggerReleased() override;
};
