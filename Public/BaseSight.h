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
	virtual AActor* GetSightActor_Implementation() const override;
	virtual TSubclassOf<UUserWidget> GetAimingCrosshair_Implementation() const override;
	virtual bool ShouldHideFPSMeshWhenAiming_Implementation() const override;
	virtual float GetAimingFOV_Implementation() const override;
	virtual float GetAimLookSpeed_Implementation() const override;
	virtual float GetAimLeaningScale_Implementation() const override;
	virtual float GetAimBreathingScale_Implementation() const override;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight")
	TSubclassOf<UUserWidget> AimCrossHair;

	// Camera FOV when aiming down sights
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight")
	float AimFOV = 90.0f;

	// Look speed multiplier when aiming (0.5 = half speed, 1.0 = normal speed)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight")
	float AimLookSpeed = 1.0f;

	// Leaning scale when aiming (0.0 = no lean, 1.0 = full lean)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight")
	float AimLeaningScale = 1.0f;

	// Breathing scale when aiming (0.0 = no breathing, 1.0 = full breathing)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight")
	float AimBreathingScale = 0.3f;

	// Hide FPS weapon mesh when aiming (true for sniper scopes, false for iron sights/red dots)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight")
	bool bHideFPSMeshWhenAiming = false;
};
