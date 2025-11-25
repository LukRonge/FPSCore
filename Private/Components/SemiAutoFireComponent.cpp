// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SemiAutoFireComponent.h"
#include "TimerManager.h"

void USemiAutoFireComponent::TriggerPulled()
{
	if (CanFire() && !bTriggerHeld && !GetWorld()->GetTimerManager().IsTimerActive(FireRateTimer))
	{
		bTriggerHeld = true;
		Fire();

		GetWorld()->GetTimerManager().SetTimer(
			FireRateTimer,
			GetTimeBetweenShots(),
			false
		);
	}
}

void USemiAutoFireComponent::TriggerReleased()
{
	bTriggerHeld = false;
}
