// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ThrowRelease.generated.h"

/**
 * UAnimNotify_ThrowRelease
 *
 * Animation notify that triggers grenade throw release.
 * Place at the release frame in ThrowMontage.
 *
 * Timeline Position: Frame where grenade leaves hand
 *
 * Action:
 * - Navigates from AnimInstance -> Character -> ActiveItem
 * - Calls ABaseGrenade::OnThrowNotify()
 * - OnThrowNotify gets spawn location/direction and calls Server_ExecuteThrow
 * - LOCAL operation (runs on owning client, triggers server RPC)
 *
 * Navigation Chain:
 * MeshComp -> Owner (AFPSCharacter)
 *          -> IItemCollectorInterface::GetActiveItem()
 *          -> ABaseGrenade::OnThrowNotify()
 *
 * Usage in Animation:
 * - Add this notify to throw animation montage
 * - Place at exact frame where grenade should be released
 * - Animation continues with follow-through after notify
 *
 * Grenade Throw Flow:
 * 1. UseStart -> Play ThrowMontage
 * 2. This notify fires at release frame
 * 3. OnThrowNotify -> Server_ExecuteThrow
 * 4. Server spawns AGrenadeProjectile
 * 5. Animation continues through follow-through
 */
UCLASS()
class FPSCORE_API UAnimNotify_ThrowRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("ThrowRelease");
	}
};
