// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/FullAutoFireComponent.h"
#include "TimerManager.h"

void UFullAutoFireComponent::TriggerPulled()
{
	// Full-auto: Fire first shot immediately, then start timer
	if (CanFire())
	{
		bTriggerHeld = true;

		// Fire first shot immediately (instant response)
		Fire();

		// Start timer for continuous fire
		// Interval = 60.0 / FireRate (converts RPM to seconds per shot)
		GetWorld()->GetTimerManager().SetTimer(
			FireRateTimer,
			this,
			&UFullAutoFireComponent::Fire,
			GetTimeBetweenShots(),
			true // Loop
		);

		UE_LOG(LogTemp, Log, TEXT("FullAutoFireComponent::TriggerPulled() - Started continuous fire at %.0f RPM"),
			FireRate);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("FullAutoFireComponent::TriggerPulled() - CanFire() returned false"));
	}
}

void UFullAutoFireComponent::TriggerReleased()
{
	// Stop continuous fire
	bTriggerHeld = false;

	// Clear timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireRateTimer);
	}

	UE_LOG(LogTemp, Log, TEXT("FullAutoFireComponent::TriggerReleased() - Stopped continuous fire"));
}

void UFullAutoFireComponent::Fire()
{
	// Check if still can fire (ammo available)
	if (!CanFire())
	{
		// Out of ammo - stop firing
		TriggerReleased();

		UE_LOG(LogTemp, Log, TEXT("FullAutoFireComponent::Fire() - Out of ammo, stopping fire"));
		return;
	}

	// Call parent Fire() implementation
	Super::Fire();
}
