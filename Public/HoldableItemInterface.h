// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HoldableItemInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UHoldableItemInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IHoldableItemInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item")
	FVector GetHandsOffset();
};
