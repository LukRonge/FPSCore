// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SemiAutoFireComponent.h"

void USemiAutoFireComponent::TriggerPulled()
{
	// Semi-auto: Fire one shot if trigger not already held
	if (CanFire() && !bTriggerHeld)
	{
		// Mark trigger as held (prevents spam clicking)
		bTriggerHeld = true;

		// Fire single shot
		Fire();

		UE_LOG(LogTemp, Log, TEXT("SemiAutoFireComponent::TriggerPulled() - Shot fired"));
	}
	else if (bTriggerHeld)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SemiAutoFireComponent::TriggerPulled() - Trigger already held, release first"));
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("SemiAutoFireComponent::TriggerPulled() - CanFire() returned false"));
	}
}

void USemiAutoFireComponent::TriggerReleased()
{
	// Reset trigger state - allow next shot
	bTriggerHeld = false;

	UE_LOG(LogTemp, Verbose, TEXT("SemiAutoFireComponent::TriggerReleased() - Ready for next shot"));
}
