// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "M72A7_Law.generated.h"

class UDisposableComponent;

/**
 * M72A7 LAW (Light Anti-tank Weapon)
 * Single-use disposable rocket launcher
 *
 * SPECS:
 * - Type: Disposable anti-tank rocket launcher
 * - Caliber: 66mm HEAT rocket
 * - Capacity: 1 round (pre-loaded, non-reloadable)
 * - Effective Range: 200m (moving targets), 300m (stationary)
 *
 * M72A7-SPECIFIC MECHANICS:
 * - Single shot weapon - fires once then discarded
 * - No magazine system (rocket is integral to launcher)
 * - No reload capability
 * - Spawns projectile actor on fire (not hitscan)
 * - After firing: plays ShootMontage → DropMontage → auto-drop
 *
 * COMPONENT ARCHITECTURE:
 * - NO BallisticsComponent (projectile-based, not hitscan)
 * - NO FireComponent (single-shot logic handled internally)
 * - NO ReloadComponent (disposable, non-reloadable)
 * - DisposableComponent: Handles drop sequence after firing
 *
 * ANIMATION SEQUENCE:
 * 1. Player fires (UseStart) → Server_Fire()
 * 2. Server spawns projectile at "barrel" socket
 * 3. ShootMontage plays on character meshes
 * 4. AnimNotify_StartDropSequence at end of ShootMontage
 * 5. DisposableComponent plays DropMontage
 * 6. AnimNotify_DropWeapon at end of DropMontage
 * 7. DisposableComponent calls IItemCollectorInterface::Drop
 *
 * STATE VARIABLES:
 * - bHasFired: Prevents multiple shots (REPLICATED)
 *
 * BLUEPRINT SETUP:
 * 1. Set skeletal meshes (FPSMesh, TPSMesh)
 * 2. Set ProjectileClass (BP_Rocket or similar)
 * 3. Set ShootMontage (character firing animation with AnimNotify_StartDropSequence at end)
 * 4. Set DisposableComponent->DropMontage (with AnimNotify_DropWeapon at end)
 * 5. Configure AnimLayer for weapon-specific poses
 */
UCLASS()
class FPSCORE_API AM72A7_Law : public ABaseWeapon
{
	GENERATED_BODY()

public:
	AM72A7_Law();

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// M72A7 STATE (Replicated)
	// ============================================

	/**
	 * Has weapon been fired?
	 * - false: Ready to fire (fresh weapon)
	 * - true: Already fired, in drop sequence or dropped
	 * REPLICATED: Server authoritative
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HasFired, Category = "M72A7|State")
	bool bHasFired = false;

	/**
	 * Is launcher expanded (ready to fire)?
	 * - false: Collapsed (stored/holstered state)
	 * - true: Expanded (equipped and ready)
	 * REPLICATED: For late-joiners and weapon AnimBP to see correct visual state
	 *
	 * Set to true by AnimNotify_ItemEquipStart during character equip montage
	 * Weapon AnimBP reads this value and plays expand animation
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsExpanded, Category = "M72A7|State")
	bool bIsExpanded = false;

protected:
	// ============================================
	// ONREP CALLBACKS
	// ============================================

	/** Called on clients when bHasFired replicates */
	UFUNCTION()
	void OnRep_HasFired();

	/** Called on clients when bIsExpanded replicates (late-joiner support) */
	UFUNCTION()
	void OnRep_IsExpanded();

public:
	// ============================================
	// PROJECTILE CONFIGURATION
	// ============================================

	/**
	 * Projectile class to spawn when firing
	 * Set in Blueprint defaults (e.g., BP_Rocket, BP_66mmHEAT)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "M72A7|Projectile")
	TSubclassOf<AActor> ProjectileClass;

	/**
	 * Socket name on weapon mesh where projectile spawns
	 * Default: "barrel" (standard muzzle socket)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "M72A7|Projectile")
	FName ProjectileSpawnSocket = FName("barrel");

	// ============================================
	// M72A7-SPECIFIC ANIMATION
	// ============================================

	/**
	 * Item equip montage - plays on weapon meshes (FPS + TPS) during equip
	 * M72A7: Launcher expand/unfold animation
	 * Triggered by AnimNotify_ItemEquipStart in character equip montage
	 * bIsExpanded is set BEFORE/WITH this montage so AnimBP can react
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "M72A7|Animation")
	UAnimMontage* ItemEquipMontage;

	/**
	 * Item unequip montage - plays on weapon meshes (FPS + TPS) during unequip
	 * M72A7: Launcher collapse/fold animation
	 * Triggered by AnimNotify_ItemUnequipStart in character unequip montage
	 * bIsExpanded is set to false BEFORE/WITH this montage so AnimBP can react
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "M72A7|Animation")
	UAnimMontage* ItemUnequipMontage;

	// ============================================
	// USABLE INTERFACE OVERRIDES
	// ============================================

	/**
	 * Override UseStart - fire projectile
	 * Single-shot: Only fires if bHasFired == false
	 * Spawns projectile at barrel socket
	 */
	virtual void UseStart_Implementation(const FUseContext& Ctx) override;

	/**
	 * Override UseStop - no-op for single-shot weapon
	 */
	virtual void UseStop_Implementation(const FUseContext& Ctx) override;

	/**
	 * Override IsUsing - always false (instant fire, no hold)
	 */
	virtual bool IsUsing_Implementation() const override;

	// ============================================
	// AMMO CONSUMER INTERFACE OVERRIDES
	// ============================================

	/** Get current ammo (1 if not fired, 0 if fired) */
	virtual int32 GetClip_Implementation() const override;

	/** Get max capacity (always 1) */
	virtual int32 GetClipSize_Implementation() const override;

	/** Consume ammo - sets bHasFired */
	virtual int32 ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx) override;

	// ============================================
	// RELOADABLE INTERFACE OVERRIDES
	// ============================================

	/** Cannot reload - disposable weapon */
	virtual bool CanReload_Implementation() const override { return false; }

	/** No-op - cannot reload */
	virtual void Reload_Implementation(const FUseContext& Ctx) override { }

	/** Never reloading */
	virtual bool IsReloading_Implementation() const override { return false; }

	/** No magazine */
	virtual AActor* GetMagazineActor_Implementation() const override { return nullptr; }

	/** No magazine mesh */
	virtual UPrimitiveComponent* GetFPSMagazineMesh_Implementation() const override { return nullptr; }

	/** No magazine mesh */
	virtual UPrimitiveComponent* GetTPSMagazineMesh_Implementation() const override { return nullptr; }

	/** No reload component */
	virtual UReloadComponent* GetReloadComponent_Implementation() const override { return nullptr; }

	// ============================================
	// HOLDABLE INTERFACE OVERRIDES
	// ============================================

	/**
	 * Cannot unequip during drop sequence
	 * Delegates to DisposableComponent::CanBeUnequipped()
	 */
	virtual bool CanBeUnequipped_Implementation() const override;

	/**
	 * Set bIsExpanded = true when equipping
	 * Called from AnimNotify_ItemEquipStart during character equip montage
	 * Weapon AnimBP reads bIsExpanded and plays expand animation
	 */
	virtual void OnItemEquipAnimationStart_Implementation(APawn* OwnerPawn) override;

	/**
	 * Set bIsExpanded = false when unequipping
	 * Called from AnimNotify_ItemUnequipStart during character unequip montage
	 * Weapon AnimBP reads bIsExpanded and plays collapse animation
	 */
	virtual void OnItemUnequipAnimationStart_Implementation(APawn* OwnerPawn) override;

	// ============================================
	// PICKUPABLE INTERFACE OVERRIDES
	// ============================================

	/**
	 * Cannot be picked up after firing
	 * Used disposable weapons are trash - no re-pickup allowed
	 */
	virtual bool CanBePicked_Implementation(const FInteractionContext& Ctx) const override;

	/**
	 * Set bIsExpanded = false when dropped
	 * Dropped M72A7 should be in collapsed state
	 */
	virtual void OnDropped_Implementation(const FInteractionContext& Ctx) override;

protected:
	// ============================================
	// COMPONENTS
	// ============================================

	/**
	 * Disposable component - handles drop sequence after firing
	 * Configure DropMontage in Blueprint defaults
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "M72A7|Components")
	UDisposableComponent* DisposableComponent;

	// ============================================
	// SERVER RPC
	// ============================================

	/**
	 * Server RPC to fire the rocket
	 * Validates state, spawns projectile, triggers effects
	 */
	UFUNCTION(Server, Reliable)
	void Server_Fire();

	// ============================================
	// BASEWEAPON OVERRIDES
	// ============================================

	/**
	 * Override Multicast_PlayShootEffects for M72A7-specific behavior
	 * Spawns muzzle VFX, plays ShootMontage
	 * NOTE: No UFUNCTION - inherited from BaseWeapon
	 */
	virtual void Multicast_PlayShootEffects_Implementation() override;

	// ============================================
	// HELPERS
	// ============================================

	/**
	 * Spawn projectile at barrel socket (SERVER ONLY)
	 * @return Spawned projectile actor or nullptr on failure
	 */
	AActor* SpawnProjectile();
};
