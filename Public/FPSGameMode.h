// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/GameModeDeathInterface.h"
#include "FPSGameMode.generated.h"

UCLASS()
class FPSCORE_API AFPSGameMode : public AGameModeBase, public IGameModeDeathInterface
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
	void SpawnPlayerCharacter(APlayerController* PC);

	// IGameModeDeathInterface
	virtual void OnPlayerDeath_Implementation(AController* PlayerController, APawn* DeadPawn, AActor* Killer) override;

	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void RespawnPlayer(AController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Respawn")
	bool FindRespawnLocation(FVector& OutLocation);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	float RespawnDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	FVector SpawnAreaCenter = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	FVector SpawnAreaHalfExtents = FVector(10000.0f, 10000.0f, 100.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	float SpawnTraceHeight = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	float SpawnGroundOffset = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	int32 MaxSpawnAttempts = 10;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Game")
	TArray<APlayerController*> PlayerControllers;

	UPROPERTY(BlueprintReadOnly, Category = "Game")
	int32 PlayerIDs = 0;

private:
	TMap<AController*, FTimerHandle> PendingRespawnTimers;
};
