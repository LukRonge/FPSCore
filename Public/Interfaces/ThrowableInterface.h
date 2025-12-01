// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ThrowableInterface.generated.h"

/**
 * CAPABILITY: Throwable
 * Interface for items that can be thrown (grenades, knives, etc.)
 *
 * Implemented by: ABaseGrenade, throwable melee weapons
 * Design: Provides throw capability without direct class casts
 *
 * Used by: AnimNotify_ThrowRelease to trigger throw release
 * via interface call instead of direct Cast<ABaseGrenade>
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UThrowableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IThrowableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called when throw animation reaches release point
	 * Typically from AnimNotify_ThrowRelease
	 *
	 * Implementation should:
	 * - Get spawn location from mesh socket
	 * - Get throw direction from camera
	 * - Call server RPC to execute throw
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Throwable")
	void OnThrowRelease();
	virtual void OnThrowRelease_Implementation() { }

	/**
	 * Check if item can be thrown right now
	 * @return true if throw is allowed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Throwable")
	bool CanThrow() const;
	virtual bool CanThrow_Implementation() const { return true; }

	/**
	 * Check if item has already been thrown
	 * @return true if already thrown (single-use items)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Throwable")
	bool HasBeenThrown() const;
	virtual bool HasBeenThrown_Implementation() const { return false; }
};
