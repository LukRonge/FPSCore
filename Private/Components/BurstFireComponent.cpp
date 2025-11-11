// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BurstFireComponent.h"
#include "TimerManager.h"

void UBurstFireComponent::TriggerPulled()
{
	// Burst: Fire burst if not already firing
	if (CanFire() && !bTriggerHeld && !bBurstActive)
	{
		bTriggerHeld = true;
		bBurstActive = true;
		CurrentBurstShot = 0;

		// Fire first shot immediately
		Fire();

		UE_LOG(LogTemp, Log, TEXT("BurstFireComponent::TriggerPulled() - Started %d-round burst"),
			BurstCount);
	}
	else if (bBurstActive)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BurstFireComponent::TriggerPulled() - Burst already active, wait for completion"));
	}
	else if (bTriggerHeld)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BurstFireComponent::TriggerPulled() - Trigger already held, release first"));
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("BurstFireComponent::TriggerPulled() - CanFire() returned false"));
	}
}

void UBurstFireComponent::TriggerReleased()
{
	// Reset trigger state (burst continues regardless)
	bTriggerHeld = false;

	UE_LOG(LogTemp, Verbose, TEXT("BurstFireComponent::TriggerReleased() - Trigger released (burst may still be active)"));
}

void UBurstFireComponent::Fire()
{
	// Check if still can fire (ammo available)
	if (!CanFire())
	{
		// Out of ammo - stop burst
		bBurstActive = false;
		CurrentBurstShot = 0;

		UE_LOG(LogTemp, Log, TEXT("BurstFireComponent::Fire() - Out of ammo, stopping burst"));
		return;
	}

	// Call parent Fire() implementation
	Super::Fire();

	// Increment burst shot counter
	CurrentBurstShot++;

	UE_LOG(LogTemp, Log, TEXT("BurstFireComponent::Fire() - Burst shot %d/%d"),
		CurrentBurstShot, BurstCount);

	// Check if burst complete
	if (CurrentBurstShot >= BurstCount)
	{
		// Burst complete - reset state
		bBurstActive = false;
		CurrentBurstShot = 0;

		UE_LOG(LogTemp, Log, TEXT("BurstFireComponent::Fire() - Burst complete"));
	}
	else
	{
		// Schedule next burst shot
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				FireRateTimer,
				this,
				&UBurstFireComponent::Fire,
				BurstDelay,
				false // One-shot timer (not looping)
			);
		}
	}
}
