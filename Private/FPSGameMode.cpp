// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSGameMode.h"
#include "FPSCharacter.h"
#include "FPSPlayerController.h"
#include "GameFramework/PlayerStart.h"

AFPSGameMode::AFPSGameMode()
{
	// Set default pawn and controller classes
	DefaultPawnClass = AFPSCharacter::StaticClass();
	PlayerControllerClass = AFPSPlayerController::StaticClass();
}

void AFPSGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AFPSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AFPSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(NewPlayer))
	{
		SpawnPlayerCharacter(PC);
	}
}

void AFPSGameMode::SpawnPlayerCharacter(AFPSPlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	// Get current controlled pawn
	APawn* OldPawn = PC->GetPawn();
	if (OldPawn)
	{
		// Unpossess and destroy old pawn
		PC->UnPossess();
		OldPawn->Destroy();
	}

	// Find a player start
	AActor* PlayerStart = FindPlayerStart(PC);
	if (!PlayerStart)
	{
		return;
	}

	// Spawn new character from FPSCharacterClass
	if (!FPSCharacterClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFPSCharacter* NewCharacter = GetWorld()->SpawnActor<AFPSCharacter>(
		FPSCharacterClass,
		PlayerStart->GetActorLocation(),
		PlayerStart->GetActorRotation(),
		SpawnParams
	);

	if (NewCharacter)
	{
		// Possess new character
		PC->Possess(NewCharacter);

		// Set input mode to game only
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}
