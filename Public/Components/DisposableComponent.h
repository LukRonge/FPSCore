// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DisposableComponent.generated.h"

/**
 * UDisposableComponent
 *
 * CAPABILITY: Disposable Item
 * Component that marks an item as single-use/disposable.
 * After use, the item plays a drop sequence and auto-discards.
 *
 * ARCHITECTURE:
 * - Attach to any actor that should be discarded after use
 * - AnimNotify_StartDropSequence finds this component via FindComponentByClass
 * - AnimNotify_DropWeapon finds this component to trigger final drop
 * - No direct class casts - fully component-based capability
 *
 * WORKFLOW:
 * 1. Owner fires weapon → sets bHasBeenUsed = true
 * 2. ShootMontage plays with AnimNotify_StartDropSequence at end
 * 3. AnimNotify finds DisposableComponent → calls StartDropSequence()
 * 4. DropMontage plays with AnimNotify_DropWeapon at end
 * 5. AnimNotify finds DisposableComponent → calls ExecuteDrop()
 * 6. Component calls IItemCollectorInterface::Drop on character
 *
 * USAGE:
 * - Add component to disposable weapon (M72A7_Law, grenades, etc.)
 * - Set DropMontage in Blueprint defaults
 * - Owner must call MarkAsUsed() after firing
 *
 * MULTIPLAYER:
 * - bHasBeenUsed is NOT replicated (visual state, each machine tracks locally)
 * - Drop action is SERVER ONLY (IItemCollectorInterface::Drop is authoritative)
 */
UCLASS(ClassGroup = (FPSCore), meta = (BlueprintSpawnableComponent))
class FPSCORE_API UDisposableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDisposableComponent();

	// ============================================
	// CONFIGURATION
	// ============================================

	/**
	 * Drop/discard montage played after item is used
	 * Played on character meshes (Body, Arms, Legs)
	 * Should have AnimNotify_DropWeapon at end
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Disposable|Animation")
	UAnimMontage* DropMontage;

	// ============================================
	// STATE
	// ============================================

	/**
	 * Has item been used? (fired, thrown, etc.)
	 * Set by owner via MarkAsUsed()
	 * NOT REPLICATED - local visual state
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Disposable|State")
	bool bHasBeenUsed = false;

	/**
	 * Is drop sequence currently in progress?
	 * Prevents re-entry and blocks unequip
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Disposable|State")
	bool bInDropSequence = false;

	// ============================================
	// PUBLIC METHODS
	// ============================================

	/**
	 * Mark item as used - call after firing/throwing
	 * Must be called before drop sequence can start
	 */
	UFUNCTION(BlueprintCallable, Category = "Disposable")
	void MarkAsUsed();

	/**
	 * Start the drop/discard sequence
	 * Called by AnimNotify_StartDropSequence
	 * Plays DropMontage on character meshes
	 */
	UFUNCTION(BlueprintCallable, Category = "Disposable")
	void StartDropSequence();

	/**
	 * Execute final drop action
	 * Called by AnimNotify_DropWeapon
	 * SERVER ONLY: Calls IItemCollectorInterface::Drop
	 */
	UFUNCTION(BlueprintCallable, Category = "Disposable")
	void ExecuteDrop();

	/**
	 * Check if item can be unequipped
	 * Returns false during drop sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Disposable")
	bool CanBeUnequipped() const;

	/**
	 * Check if item can be picked up
	 * Returns false if item has been used (disposable items cannot be re-picked after use)
	 */
	UFUNCTION(BlueprintPure, Category = "Disposable")
	bool CanBePickedUp() const;

protected:
	/**
	 * Play montage on character meshes
	 * Uses ICharacterMeshProviderInterface to get meshes
	 */
	void PlayDropMontageOnCharacter();

	/**
	 * Multicast RPC to play drop montage on all clients
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDropMontage();
};
