// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/UsableInterface.h"
#include "Interfaces/PickupableInterface.h"
#include "Interfaces/ThrowableInterface.h"
#include "BaseGrenade.generated.h"

/**
 * ABaseGrenade
 *
 * Base class for throwable grenades in the FPS framework.
 * Follows the M72A7 LAW pattern - single-use item that spawns a projectile.
 *
 * ARCHITECTURE:
 * - Does NOT inherit from ABaseWeapon (no fire/reload components needed)
 * - Implements same interfaces for seamless inventory integration
 * - Single-use: destroyed after throw (no DisposableComponent needed)
 * - Spawns AGrenadeProjectile on throw
 *
 * TWO-ACTOR PATTERN:
 * - ABaseGrenade: Holdable item in character's hand
 * - AGrenadeProjectile: Physics object thrown into world
 *
 * THROW FLOW:
 * 1. UseStart → Play ThrowMontage → Multicast to all clients
 * 2. AnimNotify_ThrowRelease → OnThrowRelease()
 * 3. Server_ExecuteThrow → Spawn projectile, set bHasThrown
 * 4. Server destroys grenade actor after successful throw
 *
 * MULTIPLAYER:
 * - bHasThrown replicated for state sync
 * - Projectile spawn is SERVER ONLY
 * - Animation playback is LOCAL on each machine
 *
 * See: Docs/BaseGrenadeArchitecture.md for full specification
 */
UCLASS(Abstract, Blueprintable)
class FPSCORE_API ABaseGrenade : public AActor,
	public IInteractableInterface,
	public IHoldableInterface,
	public IUsableInterface,
	public IPickupableInterface,
	public IThrowableInterface
{
	GENERATED_BODY()

public:
	ABaseGrenade();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_Owner() override;

	// ============================================
	// COMPONENTS
	// ============================================

	/** Scene root - hierarchy anchor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** First-person mesh - visible only to owner */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> FPSMesh;

	/** Third-person mesh - visible to other players */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> TPSMesh;

	// ============================================
	// CONFIGURATION - General
	// ============================================

	/** Display name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Config")
	FText Name;

	/** Item description */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Config")
	FText Description;

	// ============================================
	// CONFIGURATION - Projectile
	// ============================================

	/**
	 * Projectile class to spawn when thrown
	 * Must implement IProjectileInterface
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Projectile")
	TSubclassOf<AActor> ProjectileClass;

	/** Socket on FPS mesh where projectile spawns from */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Projectile")
	FName ProjectileSpawnSocket = FName("weapon_r");

	// ============================================
	// CONFIGURATION - Attachment
	// ============================================

	/** Socket on character hand for attachment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Attachment")
	FName CharacterAttachSocket = FName("weapon_r");

	// ============================================
	// CONFIGURATION - Animation
	// ============================================

	/** Animation layer class for arms */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Animation")
	TSubclassOf<UAnimInstance> ArmsAnimLayer;

	/** Equip/draw animation montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Animation")
	TObjectPtr<UAnimMontage> EquipMontage;

	/** Unequip/holster animation montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Animation")
	TObjectPtr<UAnimMontage> UnequipMontage;

	/** Throw animation montage - contains AnimNotify_ThrowRelease */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;

	/** FPS arms positioning offset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Animation")
	FVector ArmsOffset = FVector::ZeroVector;

	// ============================================
	// REPLICATED STATE
	// ============================================

	/** Has this grenade been thrown? Replicated for state sync */
	UPROPERTY(ReplicatedUsing = OnRep_HasThrown, BlueprintReadOnly, Category = "Grenade|State")
	bool bHasThrown = false;

	/** OnRep callback for bHasThrown */
	UFUNCTION()
	void OnRep_HasThrown();

	// ============================================
	// LOCAL STATE (Not Replicated)
	// ============================================

	/** Cached owning pawn reference */
	UPROPERTY(BlueprintReadOnly, Category = "Grenade|State")
	TObjectPtr<APawn> OwningPawn;

	/** Is currently equipped in hand */
	bool bIsEquipped = false;

	/** Is equip montage playing */
	bool bIsEquipping = false;

	/** Is unequip montage playing */
	bool bIsUnequipping = false;

	/** Is throw in progress (montage playing) */
	bool bIsThrowing = false;

	/** Has visibility been initialized */
	bool bVisibilityInitialized = false;

	// ============================================
	// SERVER RPC
	// ============================================

	/**
	 * Server RPC to start throw action
	 * CLIENT calls this from UseStart → SERVER validates and triggers Multicast_PlayThrowEffects
	 * This follows the same pattern as BaseWeapon::Server_Shoot
	 */
	UFUNCTION(Server, Reliable)
	void Server_StartThrow();

	/**
	 * Server RPC to execute throw
	 * Called from OnThrowRelease when AnimNotify fires
	 *
	 * @param SpawnLocation - World location to spawn projectile
	 * @param ThrowDirection - Direction to throw (normalized)
	 */
	UFUNCTION(Server, Reliable)
	void Server_ExecuteThrow(FVector SpawnLocation, FVector ThrowDirection);

	// ============================================
	// MULTICAST RPC
	// ============================================

	/**
	 * Multicast RPC to play throw effects (animation)
	 * Called from SERVER (Server_StartThrow) to sync animation across all clients
	 * Pattern matches BaseWeapon::Multicast_PlayShootEffects
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayThrowEffects();

	// ============================================
	// INTERNAL METHODS
	// ============================================

	/**
	 * Spawn projectile actor and initialize via IProjectileInterface
	 * SERVER ONLY - called from Server_ExecuteThrow
	 *
	 * @param SpawnLocation - World location to spawn
	 * @param ThrowDirection - Direction to throw (used for rotation)
	 * @return Spawned projectile or nullptr (implements IProjectileInterface)
	 */
	AActor* SpawnProjectile(FVector SpawnLocation, FVector ThrowDirection);

	/**
	 * Setup FPS/TPS mesh visibility based on owner
	 * Called when owner is set
	 */
	void SetupMeshVisibility();

	/**
	 * Reset visibility for world (when dropped)
	 */
	void ResetVisibilityForWorld();

public:
	// ============================================
	// PUBLIC METHODS
	// ============================================

	/**
	 * Get display name
	 */
	UFUNCTION(BlueprintPure, Category = "Grenade")
	FText GetDisplayName() const { return Name; }

	/**
	 * Get description
	 */
	UFUNCTION(BlueprintPure, Category = "Grenade")
	FText GetDescription() const { return Description; }

	// ============================================
	// IThrowableInterface Implementation
	// ============================================

	virtual void OnThrowRelease_Implementation() override;
	virtual bool CanThrow_Implementation() const override;
	virtual bool HasBeenThrown_Implementation() const override { return bHasThrown; }

	// ============================================
	// IHoldableInterface Implementation
	// ============================================

	virtual void OnEquipped_Implementation(APawn* NewOwner) override;
	virtual void OnUnequipped_Implementation() override;
	virtual bool IsEquipped_Implementation() const override { return bIsEquipped; }
	virtual FVector GetArmsOffset_Implementation() const override { return ArmsOffset; }
	virtual UPrimitiveComponent* GetFPSMeshComponent_Implementation() const override { return FPSMesh; }
	virtual UPrimitiveComponent* GetTPSMeshComponent_Implementation() const override { return TPSMesh; }
	virtual void SetFPSMeshVisibility_Implementation(bool bVisible) override;
	virtual FName GetAttachSocket_Implementation() const override { return CharacterAttachSocket; }
	virtual FName GetReloadAttachSocket_Implementation() const override { return CharacterAttachSocket; }
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const override { return ArmsAnimLayer; }
	virtual float GetLeaningScale_Implementation() const override { return 1.0f; }
	virtual float GetBreathingScale_Implementation() const override { return 1.0f; }
	virtual void SetAiming_Implementation(bool bAiming) override { /* Grenades don't aim */ }
	virtual bool GetIsAiming_Implementation() const override { return false; }
	virtual bool CanAim_Implementation() const override { return false; }
	virtual bool CanBeUnequipped_Implementation() const override;
	virtual UAnimMontage* GetEquipMontage_Implementation() const override { return EquipMontage; }
	virtual UAnimMontage* GetUnequipMontage_Implementation() const override { return UnequipMontage; }
	virtual bool IsEquipping_Implementation() const override { return bIsEquipping; }
	virtual bool IsUnequipping_Implementation() const override { return bIsUnequipping; }
	virtual void SetEquippingState_Implementation(bool bEquipping) override { bIsEquipping = bEquipping; }
	virtual void SetUnequippingState_Implementation(bool bUnequipping) override { bIsUnequipping = bUnequipping; }
	virtual void OnEquipMontageComplete_Implementation(APawn* NewOwner) override;
	virtual void OnUnequipMontageComplete_Implementation(APawn* NewOwner) override;
	virtual void OnItemEquipAnimationStart_Implementation(APawn* NewOwner) override { /* No item-specific equip anim */ }

	// ============================================
	// IUsableInterface Implementation
	// ============================================

	virtual bool CanUse_Implementation(const FUseContext& Ctx) const override;
	virtual void UseStart_Implementation(const FUseContext& Ctx) override;
	virtual void UseTick_Implementation(const FUseContext& Ctx) override { /* No tick needed */ }
	virtual void UseStop_Implementation(const FUseContext& Ctx) override { /* No stop needed */ }
	virtual bool IsUsing_Implementation() const override { return bIsThrowing; }

	// ============================================
	// IPickupableInterface Implementation
	// ============================================

	virtual bool CanBePicked_Implementation(const FInteractionContext& Ctx) const override;
	virtual void OnPicked_Implementation(APawn* Picker, const FInteractionContext& Ctx) override;
	virtual void OnDropped_Implementation(const FInteractionContext& Ctx) override;

	// ============================================
	// IInteractableInterface Implementation
	// ============================================

	virtual void GetVerbs_Implementation(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const override;
	virtual bool CanInteract_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const override;
	virtual void Interact_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) override;
	virtual FText GetInteractionText_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const override;
};
