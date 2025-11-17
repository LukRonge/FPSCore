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
	FVector GetHandsOffset() const;
	virtual FVector GetHandsOffset_Implementation() const { return FVector::ZeroVector; }

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

	// Get animation layer class for this item (weapon-specific animations)
	// Linked to Body, Legs, and Arms meshes when item is equipped
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Holdable")
	TSubclassOf<UAnimInstance> GetAnimLayer() const;
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const { return nullptr; }
};
