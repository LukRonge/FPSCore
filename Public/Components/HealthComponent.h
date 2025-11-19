// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

/**
 * Delegate Signatures (Owner reacts via callbacks)
 */

/**
 * Called when owner takes damage
 * No parameters - owner reacts with visual effects (animations, screen effects)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDamagedSignature);

/**
 * Called when owner dies
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSignature);

/**
 * Called when health changes (for UI updates)
 * Broadcasts on SERVER (immediate) and CLIENTS (OnRep_Health)
 * @param NewHealth - New health value
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChangedSignature, float, NewHealth);

/**
 * UHealthComponent
 *
 * Pure gameplay component for health management and damage processing.
 *
 * Architecture:
 * - Component-based design (single responsibility: health/damage STATE only)
 * - Owner-agnostic (no direct casts, no interface assumptions)
 * - Delegate-driven communication (loose coupling with owner)
 * - Server authority for all gameplay logic
 *
 * Responsibilities (CORE GAMEPLAY ONLY):
 * ✅ Health/MaxHealth state management
 * ✅ Damage calculation (bone multipliers)
 * ✅ Death state tracking
 * ✅ Event broadcasting via delegates
 * ✅ Server authority validation
 *
 * NOT Responsible For (Owner handles via delegates):
 * ❌ Visual effects (hit reactions, ragdoll, VFX)
 * ❌ UI updates (health bars, death screens)
 * ❌ Animation (montages, death animations)
 * ❌ Physics (ragdoll activation)
 * ❌ Camera effects (desaturation, shake)
 *
 * Multiplayer Pattern:
 * - Server authority for damage/health
 * - Replicated properties (Health, bIsDeath)
 * - Delegates broadcast on SERVER after state change
 * - OnRep callbacks broadcast delegates on CLIENTS
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// ============================================
	// HEALTH PROPERTIES
	// ============================================

	/**
	 * Current health value (replicated with OnRep)
	 * Server authority - only server modifies this value
	 * Clients receive updates via OnRep_Health callback
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
	float Health = 100.0f;

	/**
	 * Maximum health value
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.0f;

	/**
	 * Death state (replicated)
	 * True when health <= 0
	 */
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_IsDeath, Category = "Health")
	bool bIsDeath = false;

	/**
	 * Bone damage multipliers for hit location damage calculation
	 * Maps bone name to damage multiplier (e.g., "head" = 2.0 for headshot)
	 *
	 * Examples:
	 * - head: 2.0 (200% damage - instant kill headshot)
	 * - neck_01/02: 1.5 (150% damage - critical hit)
	 * - spine_03/04/05: 0.4-0.45 (torso - normal damage)
	 * - limbs: 0.15-0.6 (reduced damage)
	 * - fingers: 0.1-0.2 (minimal damage)
	 * - IK/helper bones: 0.0 (no damage)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TMap<FName, float> BoneDamageMultipliers;

	// ============================================
	// DELEGATES (Owner listens and reacts)
	// ============================================

	/**
	 * Broadcast when owner takes damage
	 * Owner binds to handle visual effects (hit reactions, UI updates)
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDamagedSignature OnDamaged;

	/**
	 * Broadcast when owner dies
	 * Owner binds to handle death logic (ragdoll, camera effects, respawn)
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDeathSignature OnDeath;

	/**
	 * Broadcast when health changes
	 * Owner binds to handle UI updates (health bars)
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthChangedSignature OnHealthChanged;

	// ============================================
	// DAMAGE SYSTEM (Pure gameplay logic)
	// ============================================

	/**
	 * Apply damage to owner (SERVER ONLY)
	 * Called from owner's TakeDamage() function
	 *
	 * Flow:
	 * 1. Validate server authority
	 * 2. Calculate damage (bone multipliers)
	 * 3. Update health (clamped to 0)
	 * 4. Broadcast OnHealthChanged delegate
	 * 5. Broadcast OnDamaged delegate
	 * 6. Check for death → broadcast OnDeath delegate
	 *
	 * @param DamageAmount - Amount of damage to apply
	 * @param DamageEvent - Data structure with additional damage info
	 * @param EventInstigator - Controller responsible for damage
	 * @param DamageCauser - Actor that caused damage
	 * @return Actual damage applied (after multipliers)
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

	/**
	 * Get damage multiplier for specific bone
	 * @param BoneName - Name of bone hit
	 * @return Damage multiplier (1.0 if bone not found in map)
	 */
	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetBoneDamageMultiplier(FName BoneName) const;

	/**
	 * Reset health state to default (SERVER ONLY)
	 * Resets Health = MaxHealth and bIsDeath = false
	 * Broadcasts OnHealthChanged delegate for UI update
	 *
	 * Use case: Respawn, revive, match reset
	 *
	 * Flow:
	 * 1. Validate server authority
	 * 2. Reset Health = MaxHealth
	 * 3. Reset bIsDeath = false
	 * 4. Broadcast OnHealthChanged → FPSCharacter callback
	 * 5. Replication triggers OnRep on clients
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealthState();

private:
	// ============================================
	// REPLICATION CALLBACKS
	// ============================================

	/**
	 * OnRep callback for Health
	 * Broadcasts OnHealthChanged delegate on CLIENTS when health replicates
	 */
	UFUNCTION()
	void OnRep_Health();

	/**
	 * OnRep callback for bIsDeath
	 * Broadcasts OnDeath delegate on CLIENTS when death state replicates
	 */
	UFUNCTION()
	void OnRep_IsDeath();
};
