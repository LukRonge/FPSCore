// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SightMeshProviderInterface.generated.h"

class UPrimitiveComponent;

/**
 * CAPABILITY: Sight Mesh Provider
 * Interface for actors that provide FPS/TPS mesh components (sights, scopes)
 *
 * Implemented by: ABaseSight (Blueprint child classes)
 * Used by: BaseWeapon for sight mesh attachment
 *
 * Blueprint Implementation:
 * - Override GetFPSMesh() to return FPS mesh component (OnlyOwnerSee)
 * - Override GetTPSMesh() to return TPS mesh component (OwnerNoSee)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class USightMeshProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API ISightMeshProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get FPS mesh component (visible only to owner)
	 * @return FPS mesh component or nullptr
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight|Mesh")
	UPrimitiveComponent* GetFPSMesh() const;
	virtual UPrimitiveComponent* GetFPSMesh_Implementation() const { return nullptr; }

	/**
	 * Get TPS mesh component (visible to others, not owner)
	 * @return TPS mesh component or nullptr
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight|Mesh")
	UPrimitiveComponent* GetTPSMesh() const;
	virtual UPrimitiveComponent* GetTPSMesh_Implementation() const { return nullptr; }

	/**
	 * Get socket name where sight attaches to weapon mesh
	 * @return Socket name (e.g., "attachment0")
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sight|Mesh")
	FName GetAttachSocket() const;
	virtual FName GetAttachSocket_Implementation() const { return FName("attachment0"); }
};
