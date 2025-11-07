// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "UsableInterface.generated.h"

/**
 * CAPABILITY: Usable
 * Interface for equipped items with Press/Hold/Release pattern
 *
 * Implemented by: Weapons (shooting), Tools (flashlight), Consumables (healing)
 * Design: State machine (UseStart → UseTick → UseStop)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UUsableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IUsableInterface
{
	GENERATED_BODY()

public:
	// Check if item can be used right now
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Usable")
	bool CanUse(const FUseContext& Ctx) const;
	virtual bool CanUse_Implementation(const FUseContext& Ctx) const { return true; }

	// Called when use input is PRESSED (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Usable")
	void UseStart(const FUseContext& Ctx);
	virtual void UseStart_Implementation(const FUseContext& Ctx) { }

	// Called every frame while use input is HELD (SERVER ONLY, optional)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Usable")
	void UseTick(const FUseContext& Ctx);
	virtual void UseTick_Implementation(const FUseContext& Ctx) { }

	// Called when use input is RELEASED (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Usable")
	void UseStop(const FUseContext& Ctx);
	virtual void UseStop_Implementation(const FUseContext& Ctx) { }

	// Check if item is currently being used
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Usable")
	bool IsUsing() const;
	virtual bool IsUsing_Implementation() const { return false; }
};
