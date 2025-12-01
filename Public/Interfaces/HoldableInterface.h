// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HoldableInterface.generated.h"

/**
 * CAPABILITY: Holdable
 * Interface for items that can be equipped into character's hands
 *
 * Implemented by: Weapons, Tools, Throwables, Consumables
 * Design: Minimal lifecycle (equip/unequip)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UHoldableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IHoldableInterface
{
	GENERATED_BODY()

public:
	// Called when item is equipped into character's hands
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnEquipped(APawn* Owner);
	virtual void OnEquipped_Implementation(APawn* Owner) { }

	// Called when item is unequipped (holstered or dropped)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnUnequipped();
	virtual void OnUnequipped_Implementation() { }

	// Check if item is currently equipped
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool IsEquipped() const;
	virtual bool IsEquipped_Implementation() const { return false; }

	// Get hands offset for FPS arms positioning relative to camera
	// Used by FPSCharacter to position Arms mesh when item is equipped
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	FVector GetArmsOffset() const;
	virtual FVector GetArmsOffset_Implementation() const { return FVector::ZeroVector; }

	// ============================================
	// MESH ACCESS (GENERIC - for any holdable item)
	// ============================================

	// Get FPS mesh component (visible only to local controlled player)
	// Attached to Arms mesh, only owner sees this
	// Returns nullptr if item doesn't have separate FPS mesh
	// Returns UPrimitiveComponent* because can be UStaticMeshComponent or USkeletalMeshComponent
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	UPrimitiveComponent* GetFPSMeshComponent() const;
	virtual UPrimitiveComponent* GetFPSMeshComponent_Implementation() const { return nullptr; }

	// Set FPS mesh visibility (used for aiming with scopes)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void SetFPSMeshVisibility(bool bVisible);
	virtual void SetFPSMeshVisibility_Implementation(bool bVisible) { }

	// Get TPS mesh component (visible to other players, not owner)
	// Attached to Body mesh, other players see this
	// Returns nullptr if item doesn't have separate TPS mesh
	// Returns UPrimitiveComponent* because can be UStaticMeshComponent or USkeletalMeshComponent
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	UPrimitiveComponent* GetTPSMeshComponent() const;
	virtual UPrimitiveComponent* GetTPSMeshComponent_Implementation() const { return nullptr; }

	// Get socket name for attachment (same socket for FPS and TPS)
	// Both FPS and TPS meshes attach to same socket name (e.g. "hand_r", "weapon_socket")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	FName GetAttachSocket() const;
	virtual FName GetAttachSocket_Implementation() const { return FName("hand_r"); }

	// Get socket name for reload/bolt-action attachment
	// Used by weapons that need to re-attach to different socket during reload (e.g., bolt-action rifles)
	// Default returns same as GetAttachSocket() - override for weapons with special reload handling
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	FName GetReloadAttachSocket() const;
	virtual FName GetReloadAttachSocket_Implementation() const { return FName("weapon_r"); }

	// Get animation layer class for this item (item-specific animations)
	// Linked to Body, Legs, and Arms meshes when item is equipped
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	TSubclassOf<UAnimInstance> GetAnimLayer() const;
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const { return nullptr; }

	// Get leaning scale multiplier for this item (1.0 = normal scale)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	float GetLeaningScale() const;
	virtual float GetLeaningScale_Implementation() const { return 1.0f; }

	// Get breathing scale multiplier for this item (1.0 = normal scale)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	float GetBreathingScale() const;
	virtual float GetBreathingScale_Implementation() const { return 1.0f; }

	// ============================================
	// AIMING STATE (Called by owner character)
	// ============================================

	// Set aiming state (called by FPSCharacter when aiming starts/stops)
	// Used to notify item about owner's aiming state
	// @param bAiming - True if owner is aiming, false if not aiming
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void SetAiming(bool bAiming);
	virtual void SetAiming_Implementation(bool bAiming) { }

	// Get current aiming state
	// Used by FireComponent to check if item is in ADS
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool GetIsAiming() const;
	virtual bool GetIsAiming_Implementation() const { return false; }

	/**
	 * Check if item allows aiming right now
	 * Returns false if item is busy (reloading, overheated, jammed, etc.)
	 *
	 * Used by: FPSCharacter::CanPerformAiming()
	 * Override in items that have blocking states (weapons with reload, etc.)
	 *
	 * @return true if aiming is allowed, false if blocked
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool CanAim() const;
	virtual bool CanAim_Implementation() const { return true; }

	// ============================================
	// UNEQUIP VALIDATION
	// ============================================

	/**
	 * Check if item can be unequipped/dropped right now
	 * Returns false if item is busy (reloading, playing montage, etc.)
	 *
	 * Used by: FPSCharacter::DropPressed(), Server_DropItem()
	 * Override in items that have blocking states (weapons with reload, etc.)
	 *
	 * @return true if item can be unequipped, false if blocked
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool CanBeUnequipped() const;
	virtual bool CanBeUnequipped_Implementation() const { return true; }

	// ============================================
	// EQUIP/UNEQUIP MONTAGE SYSTEM
	// ============================================

	/**
	 * Get equip animation montage for this item
	 * Played on character meshes (Body/Arms/Legs) when equipping
	 * @return Equip montage or nullptr if no animation needed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	UAnimMontage* GetEquipMontage() const;
	virtual UAnimMontage* GetEquipMontage_Implementation() const { return nullptr; }

	/**
	 * Get unequip animation montage for this item
	 * Played on character meshes (Body/Arms/Legs) when unequipping/holstering
	 * @return Unequip montage or nullptr if no animation needed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	UAnimMontage* GetUnequipMontage() const;
	virtual UAnimMontage* GetUnequipMontage_Implementation() const { return nullptr; }

	/**
	 * Check if item is currently in equipping state (montage playing)
	 * Used to block actions during equip animation
	 * @return true if equip montage is playing
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool IsEquipping() const;
	virtual bool IsEquipping_Implementation() const { return false; }

	/**
	 * Check if item is currently in unequipping state (montage playing)
	 * Used to block actions during unequip animation
	 * @return true if unequip montage is playing
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	bool IsUnequipping() const;
	virtual bool IsUnequipping_Implementation() const { return false; }

	/**
	 * Set equipping state on item
	 * Called by character when starting equip montage
	 * @param bEquipping - true when starting equip, false when complete
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void SetEquippingState(bool bEquipping);
	virtual void SetEquippingState_Implementation(bool bEquipping) { }

	/**
	 * Set unequipping state on item
	 * Called by character when starting unequip montage
	 * @param bUnequipping - true when starting unequip, false when complete
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void SetUnequippingState(bool bUnequipping);
	virtual void SetUnequippingState_Implementation(bool bUnequipping) { }

	/**
	 * Called when equip montage completes (from AnimNotify or montage end delegate)
	 * Item should now be ready to use
	 * @param Owner - Character that owns this item
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnEquipMontageComplete(APawn* Owner);
	virtual void OnEquipMontageComplete_Implementation(APawn* Owner) { }

	/**
	 * Called when unequip montage completes (from AnimNotify or montage end delegate)
	 * Item should now be hidden/holstered
	 * @param Owner - Character that owns this item
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnUnequipMontageComplete(APawn* Owner);
	virtual void OnUnequipMontageComplete_Implementation(APawn* Owner) { }

	// ============================================
	// ITEM-SPECIFIC EQUIP ANIMATION
	// ============================================

	/**
	 * Called from AnimNotify during character equip montage to trigger item's own equip animation
	 *
	 * Used for items that have their own equip animation that must play synchronized
	 * with character's equip animation. Examples:
	 * - M72A7 LAW: Launcher expands/unfolds during equip
	 * - Folding stock weapons: Stock unfolds
	 * - Multi-tools: Components extend
	 *
	 * DEFAULT: No-op (most items don't need separate equip animation)
	 *
	 * Timeline:
	 * 1. Character starts equip montage (drawing item)
	 * 2. AnimNotify_ItemEquipStart fires at specific frame
	 * 3. This method is called → item plays its own montage
	 * 4. Both animations complete together
	 * 5. AnimNotify_EquipReady fires → item ready
	 *
	 * LOCAL operation - each machine plays animation independently
	 *
	 * @param Owner - Character that owns this item
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnItemEquipAnimationStart(APawn* Owner);
	virtual void OnItemEquipAnimationStart_Implementation(APawn* Owner) { }

	/**
	 * Called from AnimNotify during character unequip montage to trigger item's own unequip animation
	 *
	 * Used for items that have their own unequip animation that must play synchronized
	 * with character's unequip animation. Examples:
	 * - M72A7 LAW: Launcher collapses/folds during unequip
	 * - Folding stock weapons: Stock folds
	 * - Multi-tools: Components retract
	 *
	 * DEFAULT: No-op (most items don't need separate unequip animation)
	 *
	 * Timeline:
	 * 1. Character starts unequip montage (holstering item)
	 * 2. AnimNotify_ItemUnequipStart fires at specific frame
	 * 3. This method is called → item plays its own collapse montage
	 * 4. Both animations complete together
	 * 5. AnimNotify_UnequipFinished fires → item holstered
	 *
	 * LOCAL operation - each machine plays animation independently
	 *
	 * @param Owner - Character that owns this item
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	void OnItemUnequipAnimationStart(APawn* Owner);
	virtual void OnItemUnequipAnimationStart_Implementation(APawn* Owner) { }
};
