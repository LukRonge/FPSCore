// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

// Event delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemAdded, AActor*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemRemoved, AActor*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryCleared);

/**
 * Inventory Component
 * Manages item storage for characters, NPCs, containers, etc.
 *
 * Responsibilities:
 * - Store items in replicated array
 * - Add/remove items with validation
 * - Query inventory contents
 * - Broadcast events on inventory changes
 *
 * Network: Items array is replicated automatically
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// Replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// INVENTORY MANAGEMENT
	// ============================================

	/**
	 * Add item to inventory
	 * @param Item - Item to add
	 * @return true if successfully added, false if failed (inventory full, item already exists, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(AActor* Item);

	/**
	 * Remove item from inventory
	 * @param Item - Item to remove
	 * @return true if successfully removed, false if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(AActor* Item);

	/**
	 * Check if item exists in inventory
	 * @param Item - Item to check
	 * @return true if item is in inventory
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool ContainsItem(AActor* Item) const;

	/**
	 * Get item at specific index
	 * @param Index - Array index
	 * @return Item at index, or nullptr if invalid index
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	AActor* GetItemAtIndex(int32 Index) const;

	/**
	 * Get number of items in inventory
	 * @return Item count
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const;

	/**
	 * Get next holdable item in inventory (for weapon switching)
	 * @param CurrentItem - Current active item (nullptr to get first holdable)
	 * @return Next holdable item, or nullptr if none found
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	AActor* GetNextHoldableItem(AActor* CurrentItem) const;

	/**
	 * Clear entire inventory
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearInventory();

	// ============================================
	// INVENTORY DATA
	// ============================================

	// Items stored in inventory (replicated)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	TArray<AActor*> Items;

	// Maximum number of items allowed (0 = unlimited)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 MaxSlots = 0;

	// ============================================
	// EVENTS
	// ============================================

	// Called when item is added to inventory
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemAdded OnItemAdded;

	// Called when item is removed from inventory
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemRemoved OnItemRemoved;

	// Called when inventory is cleared
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryCleared OnInventoryCleared;
};
