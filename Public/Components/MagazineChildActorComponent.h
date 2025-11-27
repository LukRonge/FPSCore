// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ChildActorComponent.h"
#include "MagazineChildActorComponent.generated.h"

class ABaseMagazine;

/**
 * Magazine Child Actor Component
 *
 * Specialized ChildActorComponent for magazine with FirstPersonPrimitiveType configuration.
 *
 * RESPONSIBILITIES:
 * - Hold FirstPersonPrimitiveType (EditDefaultsOnly)
 * - Initialize visibility on child actor (ABaseMagazine) in BeginPlay
 *
 * USAGE in BaseWeapon Blueprint:
 * - FPSMagazineComponent: FirstPersonPrimitiveType = FirstPerson
 * - TPSMagazineComponent: FirstPersonPrimitiveType = WorldSpaceRepresentation
 *
 * ABaseMagazine remains UNCHANGED - this component only configures visibility.
 */
UCLASS(ClassGroup=(FPSCore), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UMagazineChildActorComponent : public UChildActorComponent
{
	GENERATED_BODY()

public:
	UMagazineChildActorComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// CONFIGURATION (EditDefaultsOnly)
	// ============================================

	/**
	 * First-person primitive type for visibility control
	 * Applied to child actor (ABaseMagazine) in BeginPlay
	 *
	 * - FirstPerson: OnlyOwnerSee = true (FPS magazine)
	 * - WorldSpaceRepresentation: OwnerNoSee = true (TPS magazine)
	 *
	 * Set in Blueprint defaults:
	 * - FPSMagazineComponent -> FirstPerson
	 * - TPSMagazineComponent -> WorldSpaceRepresentation
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Magazine")
	EFirstPersonPrimitiveType FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

	// ============================================
	// VISIBILITY INITIALIZATION
	// ============================================

	/**
	 * Initialize visibility on child actor
	 * Sets FirstPersonPrimitiveType on ABaseMagazine and calls ApplyVisibilityToMeshes()
	 *
	 * Called automatically from BeginPlay()
	 * Can be called manually after child actor is spawned
	 */
	UFUNCTION(BlueprintCallable, Category = "Magazine")
	void InitializeChildActorVisibility();

	// ============================================
	// OWNER PROPAGATION
	// ============================================

	/**
	 * Propagate owner to child actor for correct visibility
	 * Required for OnlyOwnerSee/OwnerNoSee to work
	 *
	 * @param NewOwner - New owner actor (character)
	 */
	UFUNCTION(BlueprintCallable, Category = "Magazine")
	void PropagateOwner(AActor* NewOwner);
};
