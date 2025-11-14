// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Interfaces/HoldableInterface.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Enable replication
	SetIsReplicatedByDefault(true);
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate Items array to all clients
	DOREPLIFETIME(UInventoryComponent, Items);
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ============================================
// INVENTORY MANAGEMENT
// ============================================

bool UInventoryComponent::AddItem(AActor* Item)
{
	// Validation
	if (!Item)
	{
		return false;
	}

	// Check if item already exists
	if (Items.Contains(Item))
	{
		return false;
	}

	// Check if inventory is full (0 = unlimited)
	if (MaxSlots > 0 && Items.Num() >= MaxSlots)
	{
		return false;
	}

	// Add item to array
	Items.Add(Item);

	// Broadcast event
	OnItemAdded.Broadcast(Item);

	return true;
}

bool UInventoryComponent::RemoveItem(AActor* Item)
{
	// Validation
	if (!Item)
	{
		return false;
	}

	// Check if item exists
	if (!Items.Contains(Item))
	{
		return false;
	}

	// Remove item from array
	Items.Remove(Item);

	// Broadcast event
	OnItemRemoved.Broadcast(Item);

	return true;
}

bool UInventoryComponent::ContainsItem(AActor* Item) const
{
	return Items.Contains(Item);
}

AActor* UInventoryComponent::GetItemAtIndex(int32 Index) const
{
	// Validate index
	if (!Items.IsValidIndex(Index))
	{
		return nullptr;
	}

	return Items[Index];
}

int32 UInventoryComponent::GetItemCount() const
{
	return Items.Num();
}

AActor* UInventoryComponent::GetNextHoldableItem(AActor* CurrentItem) const
{
	// If no items in inventory, return nullptr
	if (Items.Num() == 0)
	{
		return nullptr;
	}

	// If CurrentItem is nullptr, return first holdable item
	if (CurrentItem == nullptr)
	{
		for (AActor* Item : Items)
		{
			if (Item && Item->Implements<UHoldableInterface>())
			{
				return Item;
			}
		}
		return nullptr;
	}

	// Find current item index
	int32 CurrentIndex = Items.Find(CurrentItem);
	if (CurrentIndex == INDEX_NONE)
	{
		// Current item not in inventory, return first holdable
		for (AActor* Item : Items)
		{
			if (Item && Item->Implements<UHoldableInterface>())
			{
				return Item;
			}
		}
		return nullptr;
	}

	// Search for next holdable item (wrap around)
	int32 SearchIndex = CurrentIndex + 1;
	for (int32 i = 0; i < Items.Num(); i++)
	{
		// Wrap around
		if (SearchIndex >= Items.Num())
		{
			SearchIndex = 0;
		}

		AActor* Item = Items[SearchIndex];
		if (Item && Item->Implements<UHoldableInterface>())
		{
			return Item;
		}

		SearchIndex++;
	}

	// No other holdable item found, return current item
	return CurrentItem;
}

void UInventoryComponent::ClearInventory()
{
	// Clear array
	Items.Empty();

	// Broadcast event
	OnInventoryCleared.Broadcast();
}
