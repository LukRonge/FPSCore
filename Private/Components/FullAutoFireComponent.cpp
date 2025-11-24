// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/FullAutoFireComponent.h"
#include "TimerManager.h"

void UFullAutoFireComponent::TriggerPulled()
{
	if (CanFire())
	{
		bTriggerHeld = true;
		Fire();

		GetWorld()->GetTimerManager().SetTimer(
			FireRateTimer,
			this,
			&UFullAutoFireComponent::Fire,
			GetTimeBetweenShots(),
			true
		);
	}
}

void UFullAutoFireComponent::TriggerReleased()
{
	bTriggerHeld = false;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireRateTimer);
	}
}

void UFullAutoFireComponent::Fire()
{
	if (!CanFire())
	{
		TriggerReleased();
		return;
	}

	Super::Fire();
}
