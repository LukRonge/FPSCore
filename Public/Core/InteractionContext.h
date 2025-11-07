// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InteractionContext.generated.h"

/**
 * Context structure for world interactions
 * Used by IInteractable, IPickupable, ILockable
 * Contains all information needed to perform an interaction
 */
USTRUCT(BlueprintType)
struct FPSCORE_API FInteractionContext
{
	GENERATED_BODY()

	// Controller initiating the interaction
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AController> Controller = nullptr;

	// Pawn performing the interaction
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<APawn> Pawn = nullptr;

	// Instigator actor (usually same as Pawn)
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AActor> Instigator = nullptr;

	// Hit result from interaction trace
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FHitResult Hit;

	// Interaction verb (Interact.Open, Interact.Pickup, etc.)
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FGameplayTag Verb;

	// Optional item held in hand during interaction
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UObject> OptionalItem = nullptr;

	// Helper: Check if we're on server
	bool IsServer(const UObject* WorldContext) const
	{
		if (!WorldContext) return false;
		UWorld* World = WorldContext->GetWorld();
		if (!World) return false;
		return World->GetNetMode() != NM_Client;
	}

	// Helper: Check if valid
	bool IsValid() const
	{
		return Pawn != nullptr && Controller != nullptr;
	}
};

/**
 * Context structure for equipped item usage
 * Used by IUsable, IReloadable
 * Contains timing and aim information
 */
USTRUCT(BlueprintType)
struct FPSCORE_API FUseContext
{
	GENERATED_BODY()

	// Controller using the item
	UPROPERTY(BlueprintReadWrite, Category = "Use")
	TObjectPtr<AController> Controller = nullptr;

	// Pawn using the item
	UPROPERTY(BlueprintReadWrite, Category = "Use")
	TObjectPtr<APawn> Pawn = nullptr;

	// Delta time for tick operations
	UPROPERTY(BlueprintReadWrite, Category = "Use")
	float DeltaTime = 0.0f;

	// Camera/aim location
	UPROPERTY(BlueprintReadWrite, Category = "Use")
	FVector AimLocation = FVector::ZeroVector;

	// Camera/aim direction
	UPROPERTY(BlueprintReadWrite, Category = "Use")
	FVector AimDirection = FVector::ForwardVector;

	// Helper: Check if we're on server
	bool IsServer(const UObject* WorldContext) const
	{
		if (!WorldContext) return false;
		UWorld* World = WorldContext->GetWorld();
		if (!World) return false;
		return World->GetNetMode() != NM_Client;
	}

	// Helper: Check if valid
	bool IsValid() const
	{
		return Pawn != nullptr && Controller != nullptr;
	}

	// Helper: Get aim end point
	FVector GetAimEndPoint(float Distance = 50000.0f) const
	{
		return AimLocation + AimDirection * Distance;
	}
};
