// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "ReloadableInterface.generated.h"

/**
 * CAPABILITY: Reloadable
 * Interface for items that can be reloaded (weapons)
 *
 * Implemented by: Firearms
 * Design: Query-Command (CanReload + Reload)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UReloadableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IReloadableInterface
{
	GENERATED_BODY()

public:
	// Check if reload is possible right now
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable")
	bool CanReload() const;
	virtual bool CanReload_Implementation() const { return true; }

	// Start reload sequence (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable")
	void Reload(const FUseContext& Ctx);
	virtual void Reload_Implementation(const FUseContext& Ctx) { }

	// Check if currently reloading
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable")
	bool IsReloading() const;
	virtual bool IsReloading_Implementation() const { return false; }
};
