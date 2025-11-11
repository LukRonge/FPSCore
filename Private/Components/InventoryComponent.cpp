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

	DOREPLIFETIME(UInventoryComponent, Items);
}

bool UInventoryComponent::AddItem(AActor* Item)
{
	if (!Item)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				TEXT("InventoryComponent::AddItem() - Item is NULL!"));
		}
		return false;
	}

	// Check if inventory is full
	if (IsFull())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ Inventory full! Max: %d"), MaxSlots));
		}
		return false;
	}

	// Check if item already in inventory
	if (ContainsItem(Item))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ Item %s already in inventory!"), *Item->GetName()));
		}
		return false;
	}

	// Validate via Blueprint hook
	if (!CanAddItem(Item))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ CanAddItem() returned false for %s"), *Item->GetName()));
		}
		return false;
	}

	// Add to array
	int32 Index = Items.Add(Item);

	// Debug log
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			FString::Printf(TEXT("✓ InventoryComponent::AddItem() - %s | Index: %d | Count: %d"),
				*Item->GetName(), Index, Items.Num()));
	}

	// Broadcast event
	OnItemAdded.Broadcast(Item, Index);

	return true;
}

bool UInventoryComponent::RemoveItem(AActor* Item)
{
	if (!Item)
	{
		return false;
	}

	// Find item index
	int32 Index = Items.Find(Item);
	if (Index == INDEX_NONE)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ Item %s not in inventory!"), *Item->GetName()));
		}
		return false;
	}

	// Remove from array
	Items.RemoveAt(Index);

	// Debug log
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT("✓ InventoryComponent::RemoveItem() - %s | Count: %d"),
				*Item->GetName(), Items.Num()));
	}

	// Broadcast event
	OnItemRemoved.Broadcast(Item, Index);

	return true;
}

bool UInventoryComponent::ContainsItem(AActor* Item) const
{
	return Items.Contains(Item);
}

AActor* UInventoryComponent::GetItemAtIndex(int32 Index) const
{
	if (Items.IsValidIndex(Index))
	{
		return Items[Index];
	}

	return nullptr;
}

void UInventoryComponent::GetItemsByClass(TSubclassOf<AActor> ItemClass, TArray<AActor*>& OutItems) const
{
	OutItems.Empty();

	if (!ItemClass)
	{
		return;
	}

	for (AActor* Item : Items)
	{
		if (Item && Item->IsA(ItemClass))
		{
			OutItems.Add(Item);
		}
	}
}

AActor* UInventoryComponent::GetFirstItemOfClass(TSubclassOf<AActor> ItemClass) const
{
	if (!ItemClass)
	{
		return nullptr;
	}

	for (AActor* Item : Items)
	{
		if (Item && Item->IsA(ItemClass))
		{
			return Item;
		}
	}

	return nullptr;
}

void UInventoryComponent::ClearInventory()
{
	if (Items.Num() == 0)
	{
		return;
	}

	// Debug log
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			FString::Printf(TEXT("InventoryComponent::ClearInventory() - Clearing %d items"), Items.Num()));
	}

	Items.Empty();

	// Broadcast event
	OnInventoryCleared.Broadcast();
}

bool UInventoryComponent::CanAddItem_Implementation(AActor* Item) const
{
	// Default implementation - allow all items
	// Override in Blueprint or derived classes for custom validation
	return true;
}

AActor* UInventoryComponent::GetNextHoldableItem(AActor* ExcludeItem) const
{
	// Iterate through inventory to find next holdable item
	for (AActor* Item : Items)
	{
		// Skip invalid items or excluded item
		if (!Item || Item == ExcludeItem)
		{
			continue;
		}

		// Check if item implements IHoldableInterface
		if (Item->Implements<UHoldableInterface>())
		{
			return Item;
		}
	}

	// No holdable item found
	return nullptr;
}
