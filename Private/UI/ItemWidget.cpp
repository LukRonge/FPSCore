// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ItemWidget.h"

void UItemWidget::SetOwningItem(AActor* Item)
{
	// Clear previous item bindings
	if (OwningItem)
	{
		ClearWidget();
	}

	// Set new owning item
	OwningItem = Item;

	// Initial display refresh
	if (OwningItem)
	{
		RefreshDisplay();
	}
}

void UItemWidget::RefreshDisplay_Implementation()
{
	// Base implementation does nothing
	// Override in Blueprint or C++ subclass to implement display logic
}

void UItemWidget::ClearWidget()
{
	// Unbind events (override in subclass if needed)
	OwningItem = nullptr;
}

void UItemWidget::NativeDestruct()
{
	// Clean up on widget destruction
	ClearWidget();

	Super::NativeDestruct();
}
