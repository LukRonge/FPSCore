// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SemiAutoFireComponent.h"

void USemiAutoFireComponent::TriggerPulled()
{
	if (CanFire() && !bTriggerHeld)
	{
		bTriggerHeld = true;
		Fire();
	}
}

void USemiAutoFireComponent::TriggerReleased()
{
	bTriggerHeld = false;
}
