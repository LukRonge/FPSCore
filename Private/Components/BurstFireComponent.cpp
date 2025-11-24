// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BurstFireComponent.h"
#include "TimerManager.h"

void UBurstFireComponent::TriggerPulled()
{
	if (CanFire() && !bTriggerHeld && !bBurstActive)
	{
		bTriggerHeld = true;
		bBurstActive = true;
		CurrentBurstShot = 0;
		Fire();
	}
}

void UBurstFireComponent::TriggerReleased()
{
	bTriggerHeld = false;
}

void UBurstFireComponent::Fire()
{
	if (!CanFire())
	{
		bBurstActive = false;
		CurrentBurstShot = 0;
		return;
	}

	Super::Fire();
	CurrentBurstShot++;

	if (CurrentBurstShot >= BurstCount)
	{
		bBurstActive = false;
		CurrentBurstShot = 0;
	}
	else
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				FireRateTimer,
				this,
				&UBurstFireComponent::Fire,
				BurstDelay,
				false
			);
		}
	}
}
