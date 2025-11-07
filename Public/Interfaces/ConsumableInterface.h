// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "ConsumableInterface.generated.h"

/**
 * CAPABILITY: Consumable
 * Interface for items with limited charges/uses
 *
 * Implemented by: Medkits, Food, Grenades, Charges
 * Design: Charge tracking + consume function
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UConsumableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IConsumableInterface
{
	GENERATED_BODY()

public:
	// Get remaining charges/uses
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Consumable")
	int32 GetCharges() const;
	virtual int32 GetCharges_Implementation() const { return 0; }

	// Get maximum charges
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Consumable")
	int32 GetMaxCharges() const;
	virtual int32 GetMaxCharges_Implementation() const { return 1; }

	// Consume one charge (SERVER ONLY)
	// Returns: True if consumed successfully
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Consumable")
	bool ConsumeOne(const FUseContext& Ctx);
	virtual bool ConsumeOne_Implementation(const FUseContext& Ctx) { return false; }

	// Check if has any charges left
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Consumable")
	bool HasCharges() const;
	virtual bool HasCharges_Implementation() const { return GetCharges() > 0; }
};
