// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/SightInterface.h"
#include "Interfaces/SightMeshProviderInterface.h"
#include "BaseSight.generated.h"

/**
 * Base sight - Aiming configuration with mesh provider capability
 * Represents an attachable sight/scope for firearms
 *
 * ARCHITECTURE:
 * - C++ holds only aiming data (FOV, crosshair, aiming point)
 * - Blueprint child classes add FPS/TPS mesh components
 * - Mesh access via ISightMeshProviderInterface::GetFPSMesh/GetTPSMesh
 *
 * Sight is spawned as ChildActor attached to weapon's "attachment0" socket.
 * FPS mesh attaches to weapon FPSMesh, TPS mesh attaches to weapon TPSMesh.
 *
 * BLUEPRINT SETUP:
 * 1. Add FPSMesh component (StaticMesh/SkeletalMesh) - set OnlyOwnerSee = true
 * 2. Add TPSMesh component (StaticMesh/SkeletalMesh) - set OwnerNoSee = true
 * 3. Override GetFPSMesh() to return FPSMesh component
 * 4. Override GetTPSMesh() to return TPSMesh component
 */
UCLASS()
class FPSCORE_API ABaseSight : public AActor, public ISightInterface, public ISightMeshProviderInterface
{
	GENERATED_BODY()

public:
	ABaseSight();

protected:
	virtual void BeginPlay() override;

public:
	// ============================================
	// SIGHT INTERFACE (Aiming configuration)
	// ============================================

	virtual FVector GetAimingPoint_Implementation() const override;
	virtual AActor* GetSightActor_Implementation() const override;
	virtual TSubclassOf<UUserWidget> GetAimingCrosshair_Implementation() const override;
	virtual bool ShouldHideFPSMeshWhenAiming_Implementation() const override;
	virtual float GetAimingFOV_Implementation() const override;
	virtual float GetAimLookSpeed_Implementation() const override;
	virtual float GetAimLeaningScale_Implementation() const override;
	virtual float GetAimBreathingScale_Implementation() const override;

	// ============================================
	// SIGHT MESH PROVIDER INTERFACE
	// ============================================
	// NOTE: GetFPSMesh and GetTPSMesh return nullptr in base class
	// Override in Blueprint child classes to return actual mesh components

	// Get FPS mesh component (visible only to owner)
	// Override in Blueprint: return your FPSMesh component
	virtual UPrimitiveComponent* GetFPSMesh_Implementation() const override { return nullptr; }

	// Get TPS mesh component (visible to others, not owner)
	// Override in Blueprint: return your TPSMesh component
	virtual UPrimitiveComponent* GetTPSMesh_Implementation() const override { return nullptr; }

	// Get socket name where sight attaches to weapon mesh
	virtual FName GetAttachSocket_Implementation() const override { return AttachSocket; }

protected:
	// ============================================
	// ATTACHMENT CONFIGURATION
	// ============================================

	// Socket name on weapon mesh where sight attaches
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Attachment")
	FName AttachSocket = FName("attachment0");
	// ============================================
	// AIMING CONFIGURATION
	// ============================================

	// Aiming point offset relative to sight mesh (where eye looks when aiming)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Aiming")
	FVector AimingPoint = FVector::ZeroVector;

	// Crosshair widget class shown when aiming down sights
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Aiming")
	TSubclassOf<UUserWidget> AimCrossHair;

	// Camera FOV when aiming down sights
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Aiming")
	float AimFOV = 90.0f;

	// Look speed multiplier when aiming (0.5 = half speed, 1.0 = normal speed)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Aiming")
	float AimLookSpeed = 1.0f;

	// Leaning scale when aiming (0.0 = no lean, 1.0 = full lean)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Aiming")
	float AimLeaningScale = 1.0f;

	// Breathing scale when aiming (0.0 = no breathing, 1.0 = full breathing)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sight|Aiming")
	float AimBreathingScale = 0.3f;

	// Hide FPS weapon mesh when aiming (true for sniper scopes, false for iron sights/red dots)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sight|Aiming")
	bool bHideFPSMeshWhenAiming = false;
};
