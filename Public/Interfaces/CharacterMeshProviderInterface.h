// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CharacterMeshProviderInterface.generated.h"

/**
 * CAPABILITY: CharacterMeshProvider
 * Interface for characters/pawns that provide access to skeletal mesh components
 *
 * Implemented by: AFPSCharacter (multi-mesh system: Body, Arms, Legs)
 * Design: Read-only access to character meshes for animation playback
 *
 * Why needed:
 * - Components (FireComponent, ReloadComponent) need to play animations on character meshes
 * - Loose coupling: No direct cast to AFPSCharacter, no direct member access
 * - Encapsulation: Character decides which meshes to expose
 * - Modular: Works with any character implementing this interface
 *
 * Responsibility:
 * - Provide read-only access to skeletal mesh components
 * - Does NOT play animations (that's component's responsibility)
 * - Does NOT modify meshes (read-only interface)
 *
 * Usage Pattern:
 * ```cpp
 * // Component plays animation on character meshes via interface
 * if (Character->Implements<UCharacterMeshProviderInterface>())
 * {
 *     USkeletalMeshComponent* Body = ICharacterMeshProviderInterface::Execute_GetBodyMesh(Character);
 *     if (Body && Body->GetAnimInstance())
 *     {
 *         Body->GetAnimInstance()->Montage_Play(ShootMontage);
 *     }
 * }
 * ```
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UCharacterMeshProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API ICharacterMeshProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get body mesh component (third-person, visible to all players)
	 * Primary animation mesh for character (locomotion, upper body, full body anims)
	 *
	 * @return Body skeletal mesh component or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character|Mesh")
	USkeletalMeshComponent* GetBodyMesh() const;
	virtual USkeletalMeshComponent* GetBodyMesh_Implementation() const { return nullptr; }

	/**
	 * Get arms mesh component (first-person, visible only to owner)
	 * Used for first-person animations (weapon holding, reloading, etc.)
	 *
	 * @return Arms skeletal mesh component or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character|Mesh")
	USkeletalMeshComponent* GetArmsMesh() const;
	virtual USkeletalMeshComponent* GetArmsMesh_Implementation() const { return nullptr; }

	/**
	 * Get legs mesh component (first-person, visible only to owner)
	 * Used for first-person leg animations (walking, crouching, jumping)
	 *
	 * @return Legs skeletal mesh component or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character|Mesh")
	USkeletalMeshComponent* GetLegsMesh() const;
	virtual USkeletalMeshComponent* GetLegsMesh_Implementation() const { return nullptr; }
};
