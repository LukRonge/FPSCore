// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseSight.h"

ABaseSight::ABaseSight()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
}

void ABaseSight::BeginPlay()
{
	Super::BeginPlay();
}

// ============================================
// SIGHT INTERFACE IMPLEMENTATIONS
// ============================================

FTransform ABaseSight::GetAimTransform_Implementation() const
{
	// Transform AimingPoint from sight-local space to world space
	// AimingPoint is the offset relative to sight origin where camera should align
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(AimingPoint), FVector::OneVector);
}

TSubclassOf<UUserWidget> ABaseSight::GetAimingCrosshair_Implementation() const
{
	return AimCrossHair;
}

bool ABaseSight::ShouldHideFPSMeshWhenAiming_Implementation() const
{
	return bHideFPSMeshWhenAiming;
}

float ABaseSight::GetAimingFOV_Implementation() const
{
	return AimFOV;
}

float ABaseSight::GetAimLookSpeed_Implementation() const
{
	return AimLookSpeed;
}

float ABaseSight::GetAimLeaningScale_Implementation() const
{
	return AimLeaningScale;
}

float ABaseSight::GetAimBreathingScale_Implementation() const
{
	return AimBreathingScale;
}
