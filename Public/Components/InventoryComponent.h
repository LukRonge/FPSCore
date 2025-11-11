// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

/**
 * Inventory Component
 * Manages item storage for characters, NPCs, containers, etc.
 * Handles adding/removing items, validation, and events
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// INVENTORY STORAGE
	// ============================================

	// Items in inventory (replicated)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	TArray<AActor*> Items;

	// Maximum inventory slots (0 = unlimited)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	int32 MaxSlots = 0;

	// ============================================
	// CORE INVENTORY OPERATIONS
	// ============================================

	/**
	 * Add item to inventory
	 * @param Item - Actor to add
	 * @return true if successfully added
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(AActor* Item);

	/**
	 * Remove item from inventory
	 * @param Item - Actor to remove
	 * @return true if successfully removed
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(AActor* Item);

	/**
	 * Check if inventory contains item
	 * @param Item - Actor to check
	 * @return true if item is in inventory
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool ContainsItem(AActor* Item) const;

	/**
	 * Get item at specific index
	 * @param Index - Array index
	 * @return Actor at index, or nullptr if invalid
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	AActor* GetItemAtIndex(int32 Index) const;

	/**
	 * Get number of items in inventory
	 * @return Item count
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const { return Items.Num(); }

	/**
	 * Check if inventory is full
	 * @return true if at max capacity
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsFull() const { return MaxSlots > 0 && Items.Num() >= MaxSlots; }

	/**
	 * Get all items of specific class
	 * @param ItemClass - Class to filter by
	 * @param OutItems - Output array of matching items
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void GetItemsByClass(TSubclassOf<AActor> ItemClass, TArray<AActor*>& OutItems) const;

	/**
	 * Clear all items from inventory
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearInventory();

	/**
	 * Get first item of specific class
	 * @param ItemClass - Class to find
	 * @return First matching item, or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	AActor* GetFirstItemOfClass(TSubclassOf<AActor> ItemClass) const;

	/**
	 * Get next holdable item from inventory (excluding specified item)
	 * Used for auto-equip after dropping active item
	 * @param ExcludeItem - Item to exclude from search (typically current active item)
	 * @return Next holdable item, or nullptr if none found
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	AActor* GetNextHoldableItem(AActor* ExcludeItem = nullptr) const;

	// ============================================
	// EVENTS / DELEGATES
	// ============================================

	// Delegate for item added
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, AActor*, Item, int32, Index);
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemAdded OnItemAdded;

	// Delegate for item removed
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, AActor*, Item, int32, Index);
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemRemoved OnItemRemoved;

	// Delegate for inventory cleared
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryCleared);
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryCleared OnInventoryCleared;

	// ============================================
	// VALIDATION
	// ============================================

	/**
	 * Check if item can be added to inventory
	 * Blueprint implementable validation hook
	 * @param Item - Actor to validate
	 * @return true if item can be added
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory")
	bool CanAddItem(AActor* Item) const;
	virtual bool CanAddItem_Implementation(AActor* Item) const;
};
