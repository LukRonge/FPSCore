// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "LockableInterface.generated.h"

/**
 * CAPABILITY: Lockable
 * Interface for objects that can be locked/unlocked
 *
 * Implemented by: Doors, Chests, Containers
 * Design: State query + lock/unlock functions
 */
UINTERFACE(MinimalAPI, BlueprintType)
class ULockableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API ILockableInterface
{
	GENERATED_BODY()

public:
	// Check if currently locked
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lockable")
	bool IsLocked() const;
	virtual bool IsLocked_Implementation() const { return false; }

	// Try to unlock (SERVER ONLY)
	// Returns: True if successfully unlocked
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lockable")
	bool TryUnlock(const FInteractionContext& Ctx);
	virtual bool TryUnlock_Implementation(const FInteractionContext& Ctx) { return false; }

	// Lock the object (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lockable")
	void Lock(const FInteractionContext& Ctx);
	virtual void Lock_Implementation(const FInteractionContext& Ctx) { }

	// Get lock requirement description (for UI)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lockable")
	FText GetLockRequirement() const;
	virtual FText GetLockRequirement_Implementation() const { return FText::GetEmpty(); }
};
