// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RecoilHandlerInterface.generated.h"

/**
 * CAPABILITY: RecoilHandler
 * Interface for characters/pawns that handle weapon recoil application
 *
 * Implemented by: AFPSCharacter
 * Design: Component-based recoil system with multiplayer support
 *
 * Why needed:
 * - FireComponent needs to apply recoil without direct class reference
 * - Loose coupling via interface (consistent with IBallisticsHandlerInterface pattern)
 * - Multiplayer-safe: Server receives recoil request, broadcasts to all clients
 * - Separation of concerns: FireComponent (fire mechanics) → Character (visual feedback)
 *
 * Communication Pattern:
 * 1. FireComponent calls ApplyRecoilKick() on Character (via interface)
 * 2. Character adds recoil to RecoilComponent state
 * 3. Character broadcasts Multicast RPC to all clients
 * 4. Owning client: Camera kick
 * 5. Remote clients: Weapon animation
 */
UINTERFACE(MinimalAPI, BlueprintType)
class URecoilHandlerInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IRecoilHandlerInterface
{
	GENERATED_BODY()

public:
	/**
	 * Apply recoil kick from weapon firing
	 *
	 * Called by FireComponent on SERVER after shot is fired
	 * Implementation should:
	 * 1. Add recoil to RecoilComponent state (accumulation tracking)
	 * 2. Call Multicast RPC to apply visual feedback on all clients
	 *
	 * @param RecoilScale - Recoil multiplier from FireComponent property
	 *                      Example: AK47 = 1.5, M4A1 = 1.0, Pistol = 0.7
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Recoil")
	void ApplyRecoilKick(float RecoilScale);
};
