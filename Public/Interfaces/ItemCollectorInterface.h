// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemCollectorInterface.generated.h"

/**
 * CAPABILITY: Item Collector
 * Interface for actors that can pickup and drop items
 *
 * Implemented by: AFPSCharacter, AI characters, vehicles
 * Design: Actors with this capability can collect items from world
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UItemCollectorInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IItemCollectorInterface
{
	GENERATED_BODY()

public:
	/**
	 * Pickup item from world
	 * @param Item - Item to pickup
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	void Pickup(AActor* Item);
	virtual void Pickup_Implementation(AActor* Item) { }

	/**
	 * Drop item to world
	 * @param Item - Item to drop
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemCollector")
	void Drop(AActor* Item);
	virtual void Drop_Implementation(AActor* Item) { }
};
