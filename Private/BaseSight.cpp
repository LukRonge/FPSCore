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

FVector ABaseSight::GetAimingPoint_Implementation() const
{
	return AimingPoint;
}

AActor* ABaseSight::GetSightActor_Implementation() const
{
	return const_cast<ABaseSight*>(this);
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
