// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecoilComponent.generated.h"

/**
 * Recoil Component
 * Manages weapon recoil state, accumulation, and visual application
 *
 * SINGLE RESPONSIBILITY: Recoil state management and visual feedback ONLY
 *
 * DOES:
 * - Track accumulated recoil (shot count, pitch/yaw offsets)
 * - Calculate recoil kick based on accumulation curve
 * - Apply visual feedback (camera kick for owner, weapon animation for remote)
 * - Interpolate recovery back to center
 * - Apply ADS reduction modifier
 *
 * DOES NOT:
 * - Fire rate management (→ FireComponent)
 * - Spread/ballistics (→ FireComponent/BallisticsComponent)
 * - Network replication (→ FPSCharacter Multicast RPC handles broadcast)
 * - Input handling (→ FPSCharacter)
 *
 * ARCHITECTURE:
 * - NOT replicated - state tracking on all machines independently
 * - LOCAL visual feedback (camera/weapon)
 * - Called by FPSCharacter::Multicast_ApplyRecoil() on all clients
 * - Component-based capability pattern (attached to FPSCharacter)
 *
 * MULTIPLAYER:
 * - Server: Tracks state for accumulation logic
 * - Owning Client: Applies camera kick via UpdatePitch()
 * - Remote Clients: Applies weapon animation (TPS mesh)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API URecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URecoilComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================
	// DESIGNER DEFAULTS - RECOIL PATTERN
	// ============================================

	// Base vertical recoil per shot (degrees) - upward camera kick
	// Controllable: 0.4-1.2, Harsh: 2.0+
	// M4A1: 0.8, AK47: 1.2, Pistol: 0.5, Sniper: 2.0
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Pattern")
	float BaseVerticalRecoil = 0.8f;

	// Base horizontal recoil per shot (degrees, random +/-)
	// Smooth: 0.1-0.25, Moderate: 0.3-0.5, Harsh: 0.6+
	// M4A1: 0.2 (smooth), AK47: 0.4 (moderate)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Pattern")
	float BaseHorizontalRecoil = 0.2f;

	// Horizontal bias - directional pull (positive = right, negative = left)
	// Subtle: 0.05-0.1, Moderate: 0.15-0.25, Strong: 0.3+
	// M4A1: 0.08 (slight right), AK47: -0.1 (slight left)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Pattern")
	float HorizontalBias = 0.08f;

	// ============================================
	// DESIGNER DEFAULTS - ACCUMULATION
	// ============================================

	// Max shots for full accumulation multiplier (1.0x -> 1.4x)
	// Auto weapons: 8-12, Single-shot: 1 (no accumulation)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Accumulation")
	float MaxAccumulation = 8.0f;

	// ============================================
	// DESIGNER DEFAULTS - RECOVERY
	// ============================================

	// Recovery speed (degrees per second) - how fast camera returns to center
	// Subtle: 3.0-8.0, Fast: 15.0+ (arcade feel)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Recovery")
	float RecoverySpeed = 5.0f;

	// Delay before recovery starts (seconds)
	// Quick: 0.05-0.15, Delayed: 0.3+ (sluggish)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Recovery")
	float RecoveryDelay = 0.15f;

	// ============================================
	// DESIGNER DEFAULTS - ADS MODIFIER
	// ============================================

	// Recoil multiplier when ADS (1.0 = full, 0.5 = 50% reduction)
	// Iron sights: 0.5, Scopes: 0.3-0.4
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|ADS")
	float ADSRecoilMultiplier = 0.5f;

	// ============================================
	// RUNTIME STATE (LOCAL ONLY - NOT REPLICATED)
	// ============================================

	// Current accumulated vertical recoil (degrees)
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Runtime")
	float CurrentRecoilPitch = 0.0f;

	// Current accumulated horizontal recoil (degrees)
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Runtime")
	float CurrentRecoilYaw = 0.0f;

	// Shot count for accumulation calculation
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Runtime")
	int32 ShotCount = 0;

	// Time of last shot (for recovery delay)
	float LastShotTime = 0.0f;

	// ============================================
	// METHODS
	// ============================================

	/**
	 * Add recoil to state tracking (called on SERVER for accumulation)
	 * Updates ShotCount and LastShotTime
	 * Does NOT apply visual feedback (that happens in Multicast RPC)
	 *
	 * @param Scale - Recoil multiplier from FireComponent
	 */
	UFUNCTION(BlueprintCallable, Category = "Recoil")
	void AddRecoil(float Scale);

	/**
	 * Apply recoil to camera (owning client only)
	 * Calculates vertical/horizontal kick with accumulation curve
	 * Calls UpdatePitch() and AddControllerYawInput() on FPSCharacter
	 *
	 * Called by: FPSCharacter::Multicast_ApplyRecoil() when IsLocallyControlled() == true
	 *
	 * @param Scale - Recoil multiplier from FireComponent
	 */
	UFUNCTION(BlueprintCallable, Category = "Recoil")
	void ApplyRecoilToCamera(float Scale);

	/**
	 * Apply recoil to weapon animation (remote clients only)
	 * Plays weapon kick animation on TPS mesh
	 *
	 * Called by: FPSCharacter::Multicast_ApplyRecoil() when IsLocallyControlled() == false
	 *
	 * @param Scale - Recoil multiplier from FireComponent
	 */
	UFUNCTION(BlueprintCallable, Category = "Recoil")
	void ApplyRecoilToWeapon(float Scale);

	/**
	 * Reset recoil state to idle
	 * Clears accumulated recoil and shot count
	 * Useful for weapon swap or death
	 */
	UFUNCTION(BlueprintCallable, Category = "Recoil")
	void ResetRecoil();

protected:
	/**
	 * Get accumulation multiplier based on shot count
	 * Returns: 1.0 → 2.0 based on (ShotCount / MaxAccumulation)
	 */
	float GetAccumulationMultiplier() const;
};
