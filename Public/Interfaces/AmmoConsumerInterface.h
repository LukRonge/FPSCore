// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "AmmoConsumerInterface.generated.h"

/**
 * CAPABILITY: AmmoConsumer
 * Interface for items that consume ammunition
 *
 * Implemented by: Firearms
 * Design: Capability info getters + consume function
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UAmmoConsumerInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IAmmoConsumerInterface
{
	GENERATED_BODY()

public:
	// Get ammo type identifier (5.56mm, 7.62mm, 12Gauge, etc.)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoConsumer")
	FName GetAmmoType() const;
	virtual FName GetAmmoType_Implementation() const { return NAME_None; }

	// Get current ammo in clip/magazine
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoConsumer")
	int32 GetClip() const;
	virtual int32 GetClip_Implementation() const { return 0; }

	// Get maximum clip/magazine size
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoConsumer")
	int32 GetClipSize() const;
	virtual int32 GetClipSize_Implementation() const { return 0; }

	// Get total ammo in reserve
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoConsumer")
	int32 GetTotalAmmo() const;
	virtual int32 GetTotalAmmo_Implementation() const { return 0; }

	// Consume ammo and return actual consumed amount (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoConsumer")
	int32 ConsumeAmmo(int32 Requested, const FUseContext& Ctx);
	virtual int32 ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx) { return 0; }
};
