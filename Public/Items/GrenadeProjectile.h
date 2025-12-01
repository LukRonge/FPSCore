// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ProjectileInterface.h"
#include "GrenadeProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;

/**
 * AGrenadeProjectile
 *
 * Physics-based grenade projectile that explodes after fuse timer.
 * Spawned by ABaseGrenade when thrown.
 *
 * ARCHITECTURE:
 * - Replicated actor with physics simulation
 * - UProjectileMovementComponent for trajectory and bouncing
 * - Server-authoritative fuse timer and damage
 * - Multicast explosion effects
 *
 * EXPLOSION FLOW:
 * 1. InitializeThrow() sets velocity and starts fuse timer (SERVER)
 * 2. Physics simulates trajectory, bouncing off surfaces
 * 3. FuseTimer expires → OnFuseExpired() (SERVER)
 * 4. ApplyRadialDamageWithFalloff() damages nearby actors (SERVER)
 * 5. Multicast_PlayExplosionEffects() spawns VFX/sound (ALL)
 * 6. DestroyDelay timer → Destroy() (SERVER)
 *
 * MULTIPLAYER:
 * - Actor replicates to all clients (bReplicates = true)
 * - Physics replicated via UProjectileMovementComponent
 * - bHasExploded replicated for late-joiner state sync
 * - Damage is SERVER ONLY
 * - VFX is MULTICAST
 *
 * See: Docs/BaseGrenadeArchitecture.md for full specification
 */
UCLASS(Blueprintable)
class FPSCORE_API AGrenadeProjectile : public AActor,
	public IProjectileInterface
{
	GENERATED_BODY()

public:
	AGrenadeProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	// ============================================
	// COMPONENTS
	// ============================================

	/** Sphere collision for physics */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	/** Visual mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Projectile movement for physics simulation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// ============================================
	// CONFIGURATION - Explosion
	// ============================================

	/** Fuse time in seconds before explosion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float FuseTime = 3.0f;

	/** Maximum damage at explosion center */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float ExplosionDamage = 150.0f;

	/** Outer radius of explosion damage (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float ExplosionRadius = 500.0f;

	/** Inner radius with full damage (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float ExplosionInnerRadius = 100.0f;

	/** Damage falloff exponent (1.0 = linear) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float DamageFalloff = 1.0f;

	/** Minimum damage at outer edge (multiplier of ExplosionDamage) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float MinDamageMultiplier = 0.1f;

	/** Damage type class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	TSubclassOf<UDamageType> DamageTypeClass;

	/** Delay after explosion before destroying actor (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Explosion")
	float DestroyDelay = 10.0f;

	// ============================================
	// CONFIGURATION - Effects
	// ============================================

	/** Explosion particle effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Effects")
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	/** Explosion sound */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Effects")
	TObjectPtr<USoundBase> ExplosionSound;

	// ============================================
	// CONFIGURATION - Physics
	// ============================================

	/** Bounciness factor (0 = no bounce, 1 = full bounce) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Physics")
	float Bounciness = 0.3f;

	/** Friction coefficient */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Physics")
	float Friction = 0.5f;

	/** Gravity scale (1.0 = normal gravity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Physics")
	float GravityScale = 1.0f;

	// ============================================
	// REPLICATED STATE
	// ============================================

	/** Has this grenade exploded? Replicated for late-joiner state */
	UPROPERTY(ReplicatedUsing = OnRep_HasExploded, BlueprintReadOnly, Category = "Grenade|State")
	bool bHasExploded = false;

	/** OnRep callback for bHasExploded */
	UFUNCTION()
	void OnRep_HasExploded();

	// ============================================
	// LOCAL STATE
	// ============================================

	/** Pawn that threw this grenade (for damage attribution) */
	UPROPERTY(BlueprintReadOnly, Category = "Grenade|State")
	TObjectPtr<APawn> InstigatorPawn;

	/** Fuse timer handle */
	FTimerHandle FuseTimerHandle;

	/** Destroy timer handle */
	FTimerHandle DestroyTimerHandle;

	// ============================================
	// EXPLOSION METHODS (Server Only)
	// ============================================

	/**
	 * Called when fuse timer expires
	 * SERVER ONLY - applies damage and triggers effects
	 */
	UFUNCTION()
	void OnFuseExpired();

	/**
	 * Apply radial damage to nearby actors
	 * SERVER ONLY
	 */
	void ApplyExplosionDamage();

	/**
	 * Start the destroy timer
	 * SERVER ONLY
	 */
	void StartDestroyTimer();

	/**
	 * Destroy the grenade actor
	 * SERVER ONLY - replication handles client cleanup
	 */
	UFUNCTION()
	void DestroyGrenade();

	// ============================================
	// MULTICAST RPC
	// ============================================

	/**
	 * Play explosion effects on all clients
	 * Spawns VFX and plays sound
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayExplosionEffects();

public:
	// ============================================
	// IProjectileInterface Implementation
	// ============================================

	virtual void InitializeProjectile_Implementation(APawn* InInstigator) override;
	virtual bool IsProjectileActive_Implementation() const override { return !bHasExploded; }

	// ============================================
	// PUBLIC METHODS
	// ============================================

	/**
	 * Check if grenade has exploded
	 */
	UFUNCTION(BlueprintPure, Category = "Grenade")
	bool HasExploded() const { return bHasExploded; }

	/**
	 * Get time remaining on fuse
	 * Returns 0 if already exploded or timer not running
	 */
	UFUNCTION(BlueprintPure, Category = "Grenade")
	float GetFuseTimeRemaining() const;
};
