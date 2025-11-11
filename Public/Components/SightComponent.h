// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SightComponent.generated.h"

class ABaseSightActor;

/**
 * Sight Component
 * Pure sight configuration and calculation component
 *
 * SINGLE RESPONSIBILITY: Sight alignment and aiming calculations ONLY
 *
 * DOES:
 * - Store sight configuration (bones, offsets, crosshair)
 * - Calculate aiming point from rear/front sight alignment
 * - Calculate hands offset for proper sight picture
 * - Provide sight attachment data for modular sight actors
 *
 * DOES NOT:
 * - Handle weapon firing (→ FireComponent)
 * - Manage FOV changes (→ PlayerController/Camera)
 * - Apply recoil/sway (→ FireComponent)
 * - Manage crosshair UI (→ HUD/PlayerController)
 *
 * ARCHITECTURE:
 * - NOT replicated - pure configuration component
 * - Accessed by weapon/character for aiming calculations
 * - Server and client use same calculations (deterministic)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API USightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USightComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// SIGHT IDENTIFICATION
	// ============================================

	// Weapon type/model this sight is configured for
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Info")
	FName WeaponType = NAME_None;

	// Sight display name
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Info")
	FText SightName;

	// ============================================
	// AIMING POINT (Eye Position When ADS)
	// ============================================

	// Aiming point offset relative to weapon root
	// This is where the camera/eye should be positioned when aiming down sights
	// Typically aligned with rear sight position
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Alignment")
	FVector AimingPoint = FVector::ZeroVector;

	// ============================================
	// SIGHT ATTACHMENT (Modular Sight System)
	// ============================================

	// Rear sight attachment bone/socket name on weapon mesh
	// Used for attaching modular rear sight actors (iron sights, apertures)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Attachment")
	FName AttachmentRearBone = FName("attachment0");

	// Front sight attachment bone/socket name on weapon mesh
	// Used for attaching modular front sight actors (front posts, fiber optics)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Attachment")
	FName AttachmentFrontBone = FName("attachment1");

	// Rear sight offset relative to AttachmentRearBone
	// Fine-tuning for sight alignment (X=Forward, Y=Right, Z=Up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Attachment")
	FVector RearOffset = FVector::ZeroVector;

	// Front sight offset relative to AttachmentFrontBone
	// Fine-tuning for sight alignment (X=Forward, Y=Right, Z=Up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Attachment")
	FVector FrontOffset = FVector::ZeroVector;

	// ============================================
	// MODULAR SIGHT ACTORS (Optional Attachments)
	// ============================================

	// Rear sight actor class (optional - for modular sight system)
	// Set to nullptr to use weapon's built-in rear sight geometry
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Modular")
	TSubclassOf<ABaseSightActor> RearSightClass;

	// Front sight actor class (optional - for modular sight system)
	// Set to nullptr to use weapon's built-in front sight geometry
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Modular")
	TSubclassOf<ABaseSightActor> FrontSightClass;

	// Runtime rear sight actor reference (spawned from RearSightClass)
	UPROPERTY(BlueprintReadOnly, Category = "Sight|Modular")
	ABaseSightActor* RearSightActor = nullptr;

	// Runtime front sight actor reference (spawned from FrontSightClass)
	UPROPERTY(BlueprintReadOnly, Category = "Sight|Modular")
	ABaseSightActor* FrontSightActor = nullptr;

	// ============================================
	// CROSSHAIR (Aiming UI)
	// ============================================

	// Crosshair widget class shown when aiming down sights
	// Set to nullptr to use weapon's default AimCrossHair
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|UI")
	TSubclassOf<UUserWidget> AimingCrosshair;

	// ============================================
	// SIGHT PROPERTIES
	// ============================================

	// Sight magnification (1.0 = no zoom, 2.0 = 2x, 4.0 = 4x, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Properties", meta = (ClampMin = "1.0", ClampMax = "16.0"))
	float Magnification = 1.0f;

	// Should sight hide first-person weapon mesh when aiming?
	// Used for scopes/optics that obstruct weapon view
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Properties")
	bool bHideFPSMeshWhenAiming = false;

	// Field of view when aiming (degrees)
	// If zero, FOV calculated from magnification: BaseFOV / Magnification
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Properties", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float AimFOV = 0.0f;

	// ============================================
	// SIGHT CALCULATIONS
	// ============================================

	/**
	 * Calculate hands offset for aiming based on sight alignment
	 * Brings weapon into proper position so aiming point aligns with camera
	 *
	 * Formula: HandsOffset = -AimingPoint (negated to bring sight to eye)
	 * Override in Blueprint for complex sight configurations
	 *
	 * @return Hands offset vector (relative to camera)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Sight")
	FVector CalculateAimingHandsOffset() const;
	virtual FVector CalculateAimingHandsOffset_Implementation() const;

	/**
	 * Get effective FOV when aiming
	 * Uses AimFOV if set, otherwise calculates from magnification
	 *
	 * @param BaseFOV - Base field of view (hip fire FOV)
	 * @return Field of view when aiming down sights
	 */
	UFUNCTION(BlueprintPure, Category = "Sight")
	float GetEffectiveAimFOV(float BaseFOV = 90.0f) const;

	/**
	 * Get aiming point in world space
	 * Transforms AimingPoint from weapon local space to world space
	 *
	 * @return Aiming point world location
	 */
	UFUNCTION(BlueprintPure, Category = "Sight")
	FVector GetAimingPointWorldLocation() const;

	/**
	 * Get rear sight world location
	 * Transforms AttachmentRearBone + RearOffset to world space
	 *
	 * @return Rear sight world location
	 */
	UFUNCTION(BlueprintPure, Category = "Sight")
	FVector GetRearSightWorldLocation() const;

	/**
	 * Get front sight world location
	 * Transforms AttachmentFrontBone + FrontOffset to world space
	 *
	 * @return Front sight world location
	 */
	UFUNCTION(BlueprintPure, Category = "Sight")
	FVector GetFrontSightWorldLocation() const;

	// ============================================
	// MODULAR SIGHT MANAGEMENT
	// ============================================

	/**
	 * Spawn and attach modular sight actors
	 * Called in BeginPlay or when sight configuration changes
	 */
	UFUNCTION(BlueprintCallable, Category = "Sight")
	void SpawnSightActors();

	/**
	 * Destroy modular sight actors
	 * Called when sight is removed or weapon destroyed
	 */
	UFUNCTION(BlueprintCallable, Category = "Sight")
	void DestroySightActors();

	/**
	 * Update sight actor visibility based on aiming state
	 * Called when entering/exiting ADS
	 *
	 * @param bIsAiming - True if currently aiming down sights
	 */
	UFUNCTION(BlueprintCallable, Category = "Sight")
	void UpdateSightVisibility(bool bIsAiming);

private:
	// Helper: Get owner weapon skeletal mesh
	USkeletalMeshComponent* GetOwnerWeaponMesh() const;
};
