// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPSGameMode.generated.h"

class AFPSPlayerController;

UCLASS()
class FPSCORE_API AFPSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFPSGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Game")
	void SpawnPlayerCharacter(AFPSPlayerController* PC);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
	TSubclassOf<class AFPSCharacter> FPSCharacterClass;
};
