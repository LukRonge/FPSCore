// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MagazineMeshProviderInterface.generated.h"

class UPrimitiveComponent;

/**
 * CAPABILITY: Magazine Mesh Provider
 * Interface for actors that provide FPS/TPS mesh components (magazines)
 *
 * Implemented by: ABaseMagazine (Blueprint child classes)
 * Used by: BaseWeapon, BoxMagazineReloadComponent
 *
 * Blueprint Implementation:
 * - Override GetFPSMesh() to return FPS mesh component (OnlyOwnerSee)
 * - Override GetTPSMesh() to return TPS mesh component (OwnerNoSee)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UMagazineMeshProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IMagazineMeshProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get FPS mesh component (visible only to owner)
	 * @return FPS mesh component or nullptr
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Mesh")
	UPrimitiveComponent* GetFPSMesh() const;
	virtual UPrimitiveComponent* GetFPSMesh_Implementation() const { return nullptr; }

	/**
	 * Get TPS mesh component (visible to others, not owner)
	 * @return TPS mesh component or nullptr
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Mesh")
	UPrimitiveComponent* GetTPSMesh() const;
	virtual UPrimitiveComponent* GetTPSMesh_Implementation() const { return nullptr; }
};
