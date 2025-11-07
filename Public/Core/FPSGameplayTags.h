// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Gameplay Tags for FPS interaction system
 * Uses verb-based interaction pattern
 */
namespace FPSGameplayTags
{
	// ============================================
	// INTERACTION VERBS (World Objects)
	// ============================================

	// Pick up item from ground
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Pickup);

	// Open door/container
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Open);

	// Close door/container
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Close);

	// Toggle door/container state
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Toggle);

	// Unlock with key/code
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Unlock);

	// Lock door/container
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Lock);

	// Use terminal/computer
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_UseTerminal);

	// Talk to NPC
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Talk);

	// Examine object closely
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Examine);

	// Enter vehicle
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Enter);

	// Take ammo from box
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_TakeAmmo);

	// ============================================
	// USE ACTIONS (Equipped Items)
	// ============================================

	// Primary use (fire weapon, use tool)
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Use_Primary);

	// Secondary use (aim, alt fire)
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Use_Secondary);

	// Reload weapon
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Use_Reload);

	// Throw grenade/item
	FPSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Use_Throw);
}
