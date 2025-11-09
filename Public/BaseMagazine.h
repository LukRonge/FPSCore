// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/AmmoCaliberTypes.h"
#include "Components/PrimitiveComponent.h"
#include "BaseMagazine.generated.h"

/**
 * Base magazine - Pure data holder
 * Represents a detachable magazine/clip for firearms
 * Contains only essential data, no complex logic
 *
 * Visualization and behavior should be handled by ActorComponents
 */
UCLASS()
class FPSCORE_API ABaseMagazine : public AActor
{
	GENERATED_BODY()

public:
	ABaseMagazine();

protected:
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// MAGAZINE DATA
	// ============================================

	// Ammo type/caliber this magazine holds
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Magazine")
	EAmmoCaliberType AmmoType = EAmmoCaliberType::NATO_556x45mm;

	// Current ammo count in this magazine (REPLICATED)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Magazine", Replicated)
	int32 CurrentAmmo = 30;

	// Maximum capacity of this magazine
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Magazine")
	int32 MaxCapacity = 30;

	// Transform for attaching magazine to player hand (offset and rotation correction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Magazine")
	FTransform MagazineHandTransform;

	// First-person primitive type (controls mesh visibility for owner)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Magazine")
	EFirstPersonPrimitiveType FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

	// ============================================
	// SIMPLE API
	// ============================================

	/**
	 * Add ammo to magazine
	 * @param Amount - Amount to add
	 * @return Actual amount added (clamped by capacity)
	 */
	UFUNCTION(BlueprintCallable, Category = "Magazine")
	int32 AddAmmo(int32 Amount);

	/**
	 * Remove one round from magazine
	 */
	UFUNCTION(BlueprintCallable, Category = "Magazine")
	void RemoveAmmo();

	/**
	 * Initialize first-person primitive type visibility settings
	 * Sets OnlyOwnerSee/OwnerNoSee on all skeletal and static mesh components
	 */
	UFUNCTION(BlueprintCallable, Category = "Magazine")
	void InitFPSType();
};
