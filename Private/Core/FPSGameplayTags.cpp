// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/FPSGameplayTags.h"

namespace FPSGameplayTags
{
	// ============================================
	// INTERACTION VERBS
	// ============================================

	UE_DEFINE_GAMEPLAY_TAG(Interact_Pickup, "Interact.Pickup");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Open, "Interact.Open");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Close, "Interact.Close");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Toggle, "Interact.Toggle");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Unlock, "Interact.Unlock");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Lock, "Interact.Lock");
	UE_DEFINE_GAMEPLAY_TAG(Interact_UseTerminal, "Interact.UseTerminal");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Talk, "Interact.Talk");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Examine, "Interact.Examine");
	UE_DEFINE_GAMEPLAY_TAG(Interact_Enter, "Interact.Enter");
	UE_DEFINE_GAMEPLAY_TAG(Interact_TakeAmmo, "Interact.TakeAmmo");

	// ============================================
	// USE ACTIONS
	// ============================================

	UE_DEFINE_GAMEPLAY_TAG(Use_Primary, "Use.Primary");
	UE_DEFINE_GAMEPLAY_TAG(Use_Secondary, "Use.Secondary");
	UE_DEFINE_GAMEPLAY_TAG(Use_Reload, "Use.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Use_Throw, "Use.Throw");
}
