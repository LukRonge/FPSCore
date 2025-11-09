// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "InteractableInterface.generated.h"

/**
 * CAPABILITY: Interactable
 * Base interface for ALL objects that can be interacted with in the world
 * Uses verb-based system (Interact.Open, Interact.Pickup, Interact.Toggle, etc.)
 *
 * Implemented by: Doors, Items, Terminals, NPCs, Vehicles
 * Design: ONE function (Interact) with dynamic verbs, NOT multiple functions
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// Get list of available interaction verbs for this object
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void GetVerbs(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const;
	virtual void GetVerbs_Implementation(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const { }

	// Check if interaction with specific verb is allowed
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(FGameplayTag Verb, const FInteractionContext& Ctx) const;
	virtual bool CanInteract_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const { return true; }

	// Perform interaction with specific verb (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(FGameplayTag Verb, const FInteractionContext& Ctx);
	virtual void Interact_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) { }

	// Get display text for UI ("Press E to [Text]")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText(FGameplayTag Verb, const FInteractionContext& Ctx) const;
	virtual FText GetInteractionText_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const { return FText::GetEmpty(); }


};
