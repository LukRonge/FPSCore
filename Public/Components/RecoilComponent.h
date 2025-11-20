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
	// RECOIL PATTERN CONFIGURATION
	// ============================================

	/**
	 * Base vertical recoil per shot (degrees)
	 * This is the upward camera kick (pitch) when firing
	 * Controllable values: 0.4-1.2 degrees (player can compensate by pulling mouse down)
	 * Harsh values: 2.0+ degrees (too hard to control)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|Pattern")
	float BaseVerticalRecoil = 0.8f;

	/**
	 * Base horizontal recoil per shot (degrees, random ±)
	 * This is the left/right camera sway (yaw) when firing
	 *
	 * SMOOTH FEELING: 0.1-0.25° (minimal shake, vertical-focused)
	 * MODERATE: 0.3-0.5° (noticeable but controllable)
	 * HARSH: 0.6+ degrees (too much shake)
	 *
	 * For M4A1-style rifle: 0.2° (smooth, predictable)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|Pattern")
	float BaseHorizontalRecoil = 0.2f;

	/**
	 * Horizontal bias (slight pull in one direction)
	 * Adds directional tendency to horizontal recoil
	 * Positive = pull right, Negative = pull left
	 *
	 * SUBTLE: 0.05-0.1° (barely noticeable)
	 * MODERATE: 0.15-0.25° (learnable pattern)
	 * STRONG: 0.3+ degrees (too much drift)
	 *
	 * For M4A1-style rifle: 0.08° (minimal right drift)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|Pattern")
	float HorizontalBias = 0.08f;

	// ============================================
	// ACCUMULATION & RECOVERY
	// ============================================

	/**
	 * Max shots for full accumulation multiplier
	 * Recoil multiplier scales from 1.0x → 1.4x over this many shots (reduced from 2.0x)
	 * Example: MaxAccumulation=8 → Shot 1=1.0x, Shot 8=1.4x (controllable growth)
	 * Single-shot weapons (snipers): Set to 1.0 for no accumulation
	 * Lower MaxAccumulation = faster growth, Higher = slower growth
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|Accumulation")
	float MaxAccumulation = 8.0f;

	/**
	 * Recovery speed (degrees per second)
	 * How fast camera returns to center after shooting stops
	 * Subtle recovery: 3.0-8.0 degrees/second (player feels in control)
	 * Fast recovery: 15.0+ degrees/second (arcade feel)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|Recovery")
	float RecoverySpeed = 5.0f;

	/**
	 * Delay before recovery starts (seconds)
	 * Time to wait after last shot before interpolating back to center
	 * Quick start: 0.05-0.15 seconds (feels responsive)
	 * Delayed start: 0.3+ seconds (feels sluggish)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|Recovery")
	float RecoveryDelay = 0.15f;

	// ============================================
	// ADS MODIFIER
	// ============================================

	/**
	 * Recoil multiplier when aiming down sights
	 * 1.0 = full recoil, 0.5 = 50% reduction, 0.0 = no recoil
	 * Iron sights: 0.5, Scopes: 0.3-0.4
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Recoil|ADS")
	float ADSRecoilMultiplier = 0.5f;

	// ============================================
	// STATE (LOCAL ONLY - NOT REPLICATED)
	// ============================================

	/**
	 * Current accumulated vertical recoil (degrees)
	 * Tracks total pitch offset for recovery interpolation
	 * LOCAL ONLY - each machine tracks independently
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	float CurrentRecoilPitch = 0.0f;

	/**
	 * Current accumulated horizontal recoil (degrees)
	 * Tracks total yaw offset for recovery interpolation
	 * LOCAL ONLY - each machine tracks independently
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	float CurrentRecoilYaw = 0.0f;

	/**
	 * Shot count for accumulation calculation
	 * Increments with each shot, decays during recovery
	 * Used to calculate recoil multiplier curve
	 * LOCAL ONLY - each machine tracks independently
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	int32 ShotCount = 0;

	/**
	 * Time of last shot (for recovery delay)
	 * Recovery starts after (CurrentTime - LastShotTime) > RecoveryDelay
	 * LOCAL ONLY - each machine tracks independently
	 */
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
