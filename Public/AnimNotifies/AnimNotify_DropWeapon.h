// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_DropWeapon.generated.h"

/**
 * UAnimNotify_DropWeapon
 *
 * Animation notify that triggers weapon drop via DisposableComponent.
 * Place at end of DropMontage for disposable weapons.
 *
 * Timeline Position: End of drop/discard animation
 *
 * Action:
 * - Navigates from AnimInstance → Character → ActiveItem → DisposableComponent
 * - Calls DisposableComponent->ExecuteDrop()
 * - SERVER ONLY action (drop is authoritative)
 *
 * Navigation Chain:
 * MeshComp → Owner (AFPSCharacter)
 *          → IItemCollectorInterface::GetActiveItem()
 *          → FindComponentByClass<UDisposableComponent>()
 *          → ExecuteDrop() (SERVER ONLY)
 *
 * Usage in Animation:
 * - Add this notify to drop animation montage
 * - Place at frame where weapon should be released
 * - Triggers IItemCollectorInterface::Drop on character
 */
UCLASS()
class FPSCORE_API UAnimNotify_DropWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("DropWeapon");
	}
};
