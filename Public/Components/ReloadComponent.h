// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/InteractionContext.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "ReloadComponent.generated.h"

class UAnimMontage;

/**
 * UReloadComponent (Abstract Base Class)
 *
 * Core reload logic, montage playback, and ammo transfer orchestration.
 *
 * Responsibility:
 * - Reload state management (bIsReloading)
 * - Animation montage playback (Body/Arms/Legs)
 * - Montage completion tracking (OnMontageEnded delegate)
 * - Server authority for ammo transfer (OnReloadComplete)
 *
 * Design Pattern:
 * - Abstract base class with virtual functions
 * - Subclasses implement specific reload types (BoxMagazine, BoltAction, etc.)
 * - Component-based capability (weapon gains "Reloadable" by adding this component)
 *
 * Multiplayer Pattern:
 * - Server + Client replication (OnRep pattern, NO Multicast RPC)
 * - bIsReloading replicated property
 * - AnimInstance operations are LOCAL (each machine plays montage independently)
 * - Server authority for gameplay state (ammo transfer)
 */
UCLASS(Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UReloadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReloadComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// REPLICATED STATE
	// ============================================

	/**
	 * Is reload currently in progress? (REPLICATED)
	 * Server sets this to true when reload starts
	 * Clients react via OnRep_IsReloading to play animations
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsReloading, Category = "Reload")
	bool bIsReloading = false;

	// ============================================
	// CONFIGURATION
	// ============================================

	/**
	 * Reload animation montage (shared by Body, Arms, Legs)
	 * IMPORTANT: Reload montages use slot "DefaultGroup.UpperBody"
	 * This same montage will be played on all three character meshes
	 * Set this in Blueprint for specific weapon types
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload|Animation")
	UAnimMontage* ReloadMontage = nullptr;

	// ============================================
	// PUBLIC API (Called by BaseWeapon via IReloadableInterface)
	// ============================================

	/**
	 * Check if reload is possible right now
	 * Validates character state, magazine status, etc.
	 * Called by BaseWeapon::CanReload_Implementation()
	 */
	UFUNCTION(BlueprintCallable, Category = "Reload")
	virtual bool CanReload_Internal() const;

	/**
	 * Start reload sequence (SERVER RPC)
	 * Called from IReloadableInterface::Reload() on client
	 * Server validates, sets bIsReloading=true, plays montages
	 *
	 * @param Ctx - Use context with controller, pawn, aim data
	 */
	UFUNCTION(Server, Reliable, Category = "Reload")
	void Server_StartReload(const FUseContext& Ctx);

	/**
	 * Cancel reload (interrupt) (SERVER RPC)
	 * Stops montages, resets state
	 * Used for reload interruption (death, stun, weapon switch)
	 */
	UFUNCTION(Server, Reliable, Category = "Reload")
	void Server_CancelReload();

	// ============================================
	// REPLICATION CALLBACKS
	// ============================================

	/**
	 * OnRep callback for bIsReloading
	 * Runs on CLIENTS when bIsReloading replicates
	 * Starts/stops reload montages based on new state
	 */
	UFUNCTION()
	void OnRep_IsReloading();

	// ============================================
	// VIRTUAL INTERFACE (Override in Subclasses)
	// ============================================

	/**
	 * Play reload montages on character meshes (LOCAL)
	 * CRITICAL: Must bind OnMontageEnded delegate!
	 * Called from Server_StartReload (server) and OnRep_IsReloading (clients)
	 *
	 * Override in subclasses if custom montage playback needed
	 */
	virtual void PlayReloadMontages();

	/**
	 * Stop reload montages on character meshes (LOCAL)
	 * Called when reload is cancelled or interrupted
	 *
	 * Override in subclasses if custom cleanup needed
	 */
	virtual void StopReloadMontages();

	/**
	 * Called when reload montage completes
	 * Runs on SERVER + CLIENTS (montage delegate fires on all machines)
	 *
	 * @param Montage - Montage that ended
	 * @param bInterrupted - false: completed normally, true: interrupted
	 *
	 * Base implementation:
	 * - If SERVER and not interrupted: call OnReloadComplete()
	 *
	 * Override in subclasses if custom completion logic needed
	 */
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Complete reload - transfer ammo (SERVER ONLY)
	 * Called from OnMontageEnded when montage completes successfully
	 *
	 * MUST OVERRIDE in subclasses to implement ammo transfer logic
	 * Base implementation does nothing
	 */
	virtual void OnReloadComplete();

	// ============================================
	// ANIMNOTIFY CALLBACKS (Called from AnimNotify classes)
	// ============================================

	/**
	 * Called when magazine detaches from weapon (LOCAL)
	 * Triggered by UAnimNotify_MagazineOut in reload animation
	 *
	 * MUST OVERRIDE in subclasses (base does nothing)
	 * Example: Attach magazine to character hand socket
	 */
	UFUNCTION(BlueprintCallable, Category = "Reload|Notify")
	virtual void OnMagazineOut();

	/**
	 * Called when magazine re-attaches to weapon (LOCAL)
	 * Triggered by UAnimNotify_MagazineIn in reload animation
	 *
	 * MUST OVERRIDE in subclasses (base does nothing)
	 * Example: Re-attach magazine to weapon socket
	 */
	UFUNCTION(BlueprintCallable, Category = "Reload|Notify")
	virtual void OnMagazineIn();

protected:
	// ============================================
	// HELPER FUNCTIONS (INTERFACE-BASED)
	// ============================================

	/**
	 * Get owner actor (item this component is attached to)
	 * Uses GetOwner() - no casting, just returns the owner AActor*
	 * @return Owner actor or nullptr if not attached
	 */
	AActor* GetOwnerItem() const { return GetOwner(); }

	/**
	 * Get character owner actor (weapon owner)
	 * Uses GetOwner()->GetOwner() chain - no casting
	 * @return Character actor or nullptr if weapon has no owner
	 */
	AActor* GetOwnerCharacterActor() const;
};
