// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseSightActor.generated.h"

/**
 * Base Sight Actor
 * Represents modular sight attachments (iron sights, scopes, red dots, etc.)
 *
 * SINGLE RESPONSIBILITY: Visual representation of sight attachment
 *
 * DOES:
 * - Provide sight mesh (static or skeletal)
 * - Handle sight-specific VFX (reticle glow, lens reflections)
 * - Support first-person/third-person visibility control
 *
 * DOES NOT:
 * - Calculate aiming offsets (→ SightComponent)
 * - Handle weapon firing (→ FireComponent)
 * - Manage crosshair UI (→ HUD)
 *
 * USAGE:
 * - Spawned and attached by SightComponent
 * - Configured in weapon Blueprint's SightComponent
 */
UCLASS()
class FPSCORE_API ABaseSightActor : public AActor
{
	GENERATED_BODY()

public:
	ABaseSightActor();

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// COMPONENTS
	// ============================================

	// Root scene component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	// First-person sight mesh (visible only to owner)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FPSMesh;

	// Third-person sight mesh (visible to everyone except owner)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TPSMesh;

	// ============================================
	// SIGHT INFO
	// ============================================

	// Sight display name
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight")
	FText SightName;

	// Sight description
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight", meta = (MultiLine = true))
	FText SightDescription;

	// ============================================
	// SIGHT TYPE
	// ============================================

	// Is this a rear sight (true) or front sight (false)?
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight")
	bool bIsRearSight = true;

	// ============================================
	// VISIBILITY CONTROL
	// ============================================

	/**
	 * Setup owner and visibility for FPS/TPS meshes
	 * Propagates owner from weapon to sight for proper SetOnlyOwnerSee/SetOwnerNoSee
	 *
	 * @param NewOwner - Character owner (for visibility control)
	 */
	UFUNCTION(BlueprintCallable, Category = "Sight")
	void SetupOwnerAndVisibility(APawn* NewOwner);

	/**
	 * Update sight visibility based on aiming state
	 * Can be overridden for sight-specific behavior (e.g., hide rear sight aperture when ADS)
	 *
	 * @param bIsAiming - True if currently aiming down sights
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Sight")
	void UpdateAimingVisibility(bool bIsAiming);
	virtual void UpdateAimingVisibility_Implementation(bool bIsAiming);

	// ============================================
	// RETICLE / ILLUMINATION (Optional)
	// ============================================

	// Reticle material instance (for illuminated sights)
	UPROPERTY(BlueprintReadWrite, Category = "Sight|Reticle")
	UMaterialInstanceDynamic* ReticleMaterial = nullptr;

	// Reticle brightness (0.0 - 1.0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Reticle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReticleBrightness = 0.5f;

	// Reticle color
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Reticle")
	FLinearColor ReticleColor = FLinearColor::Red;

	/**
	 * Update reticle material parameters
	 * Sets brightness and color on dynamic material instance
	 */
	UFUNCTION(BlueprintCallable, Category = "Sight|Reticle")
	void UpdateReticleMaterial();
};
