// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/AmmoCaliberTypes.h"
#include "Interfaces/AmmoProviderInterface.h"
#include "Interfaces/MagazineMeshProviderInterface.h"
#include "BaseMagazine.generated.h"

/**
 * Base magazine - Pure data holder with mesh provider capability
 * Represents a detachable magazine/clip for firearms
 *
 * ARCHITECTURE:
 * - C++ holds only data (ammo count, capacity, caliber)
 * - Blueprint child classes add FPS/TPS mesh components
 * - Mesh access via IMagazineMeshProviderInterface::GetFPSMesh/GetTPSMesh
 *
 * Magazine is spawned as ChildActor attached to weapon's "magazine" socket.
 *
 * BLUEPRINT SETUP:
 * 1. Add FPSMesh component (StaticMesh/SkeletalMesh) - set OnlyOwnerSee = true
 * 2. Add TPSMesh component (StaticMesh/SkeletalMesh) - set OwnerNoSee = true
 * 3. Override GetFPSMesh() to return FPSMesh component
 * 4. Override GetTPSMesh() to return TPSMesh component
 */
UCLASS()
class FPSCORE_API ABaseMagazine : public AActor, public IAmmoProviderInterface, public IMagazineMeshProviderInterface
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

	// ============================================
	// AMMO PROVIDER INTERFACE
	// ============================================

	// Get current ammo in magazine
	virtual int32 GetCurrentAmmo_Implementation() const override { return CurrentAmmo; }

	// Set current ammo in magazine
	virtual void SetCurrentAmmo_Implementation(int32 NewAmmo) override { CurrentAmmo = FMath::Clamp(NewAmmo, 0, MaxCapacity); }

	// Add ammo to magazine and return actual amount added
	virtual int32 AddAmmoToProvider_Implementation(int32 Amount) override { return AddAmmo(Amount); }

	// Get available ammo (same as current for magazine)
	virtual int32 GetAvailableAmmo_Implementation() const override { return CurrentAmmo; }
};
