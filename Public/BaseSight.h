// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/SightInterface.h"
#include "BaseSight.generated.h"

UCLASS()
class FPSCORE_API ABaseSight : public AActor, public ISightInterface
{
	GENERATED_BODY()

public:
	ABaseSight();

protected:
	virtual void BeginPlay() override;

public:
	// Override SetOwner to propagate visibility settings to all mesh components
	virtual void SetOwner(AActor* NewOwner) override;

	// ISightInterface
	virtual FVector GetAimingPoint_Implementation() const override;
	virtual TSubclassOf<UUserWidget> GetAimingCrosshair_Implementation() const override;

	// ============================================
	// VISIBILITY CONTROL
	// ============================================

	// First-person primitive type (controls mesh visibility for owner)
	// Set by weapon during spawn:
	// - FirstPerson: Sight attached to FPSMesh (only owner sees)
	// - WorldSpaceRepresentation: Sight attached to TPSMesh (others see, owner doesn't)
	UPROPERTY(BlueprintReadWrite, Category = "Sight")
	EFirstPersonPrimitiveType FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

	/**
	 * Apply FirstPersonPrimitiveType to all mesh components
	 * Sets OnlyOwnerSee or OwnerNoSee based on FirstPersonPrimitiveType
	 * Called from SetOwner() automatically
	 */
	UFUNCTION(BlueprintCallable, Category = "Sight")
	void ApplyVisibilityToMeshes();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight")
	FVector AimingPoint = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> AimCrossHair;
};
