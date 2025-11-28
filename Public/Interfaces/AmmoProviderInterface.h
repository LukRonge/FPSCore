// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "AmmoProviderInterface.generated.h"

/**
 * CAPABILITY: AmmoProvider
 * Interface for objects that provide ammunition to consumers
 *
 * Implemented by: Ammo boxes, Ammo pickups
 * Design: Single function to transfer ammo to consumer
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UAmmoProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IAmmoProviderInterface
{
	GENERATED_BODY()

public:
	// Give ammo to consumer and return actual amount given (SERVER ONLY)
	// Returns: Actual amount of ammo transferred
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	int32 GiveAmmo(UObject* Consumer, int32 Requested, const FInteractionContext& Ctx);
	virtual int32 GiveAmmo_Implementation(UObject* Consumer, int32 Requested, const FInteractionContext& Ctx) { return 0; }

	// Get ammo type this provider supplies
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	FName GetAmmoType() const;
	virtual FName GetAmmoType_Implementation() const { return NAME_None; }

	// Get available ammo count
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	int32 GetAvailableAmmo() const;
	virtual int32 GetAvailableAmmo_Implementation() const { return 0; }

	// Get current ammo in provider (for magazine sync operations)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	int32 GetCurrentAmmo() const;
	virtual int32 GetCurrentAmmo_Implementation() const { return 0; }

	// Set current ammo in provider (for magazine sync operations)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	void SetCurrentAmmo(int32 NewAmmo);
	virtual void SetCurrentAmmo_Implementation(int32 NewAmmo) { }

	// Add ammo to provider and return actual amount added
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	int32 AddAmmoToProvider(int32 Amount);
	virtual int32 AddAmmoToProvider_Implementation(int32 Amount) { return 0; }

	// Get maximum capacity of the provider (magazine capacity)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AmmoProvider")
	int32 GetMaxCapacity() const;
	virtual int32 GetMaxCapacity_Implementation() const { return 0; }
};
