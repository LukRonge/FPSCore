// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/FireComponent.h"
#include "BurstFireComponent.generated.h"

/**
 * Burst Fire Component
 * Fixed number of rounds per trigger pull, with delay between burst shots
 *
 * FIRE MODE: Burst
 *
 * BEHAVIOR:
 * - TriggerPulled() → Fire burst (e.g. 3 rounds)
 * - Each round in burst separated by BurstDelay
 * - TriggerReleased() ignored until burst completes
 * - Subsequent TriggerPulled() ignored until burst completes
 *
 * USE CASES:
 * - Burst rifles (M16A4, AN-94, Famas)
 * - Some SMGs (M93R machine pistol)
 *
 * EXAMPLE:
 * M16A4 (3-round burst, 0.1s delay): Click → Bang-Bang-Bang (0.3s total) → Click again → Bang-Bang-Bang
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UBurstFireComponent : public UFireComponent
{
	GENERATED_BODY()

public:
	// ============================================
	// DESIGNER DEFAULTS - BURST
	// ============================================

	// Number of rounds per burst
	// M16A4: 3, AN-94 hyperburst: 2
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Burst")
	int32 BurstCount = 3;

	// Delay between shots within burst (seconds)
	// 0.1s = 600 RPM burst rate
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Burst")
	float BurstDelay = 0.1f;

	// ============================================
	// FIRE CONTROL OVERRIDE
	// ============================================

	/**
	 * Trigger pulled - start burst if not already firing
	 */
	virtual void TriggerPulled() override;

	/**
	 * Trigger released - reset trigger state (burst continues regardless)
	 */
	virtual void TriggerReleased() override;

protected:
	/**
	 * Fire override - handles burst sequence
	 */
	virtual void Fire() override;

	// ============================================
	// RUNTIME STATE
	// ============================================

	// Current shot within burst (0 to BurstCount-1)
	UPROPERTY(BlueprintReadOnly, Category = "Fire|Runtime")
	int32 CurrentBurstShot = 0;

	// Is burst currently active?
	UPROPERTY(BlueprintReadOnly, Category = "Fire|Runtime")
	bool bBurstActive = false;
};
