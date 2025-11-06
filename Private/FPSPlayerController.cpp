// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSPlayerController.h"
#include "FPSCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

AFPSPlayerController::AFPSPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Input mapping context is set up in OnPossess() only
}

void AFPSPlayerController::SetupInputMapping()
{
	// DEBUG: SetupInputMapping called
	UE_LOG(LogTemp, Warning, TEXT("[SetupInputMapping] Called"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Orange,
			TEXT("[SetupInputMapping] Called"));
	}

	// Add Input Mapping Context for Enhanced Input
	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Error, TEXT("[SetupInputMapping] NOT LocalController - ABORT"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				TEXT("[SetupInputMapping] NOT LocalController - ABORT"));
		}
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[SetupInputMapping] Subsystem is NULL - ABORT"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				TEXT("[SetupInputMapping] Subsystem is NULL - ABORT"));
		}
		return;
	}

	// DEBUG: Subsystem found
	UE_LOG(LogTemp, Display, TEXT("[SetupInputMapping] Subsystem OK"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
			TEXT("[SetupInputMapping] Subsystem OK"));
	}

	if (InputMappingContext)
	{
		UE_LOG(LogTemp, Display, TEXT("[SetupInputMapping] InputMappingContext is valid: %s"), *InputMappingContext->GetName());

		// Check if mapping context is already added to prevent duplicates
		if (!Subsystem->HasMappingContext(InputMappingContext))
		{
			Subsystem->AddMappingContext(InputMappingContext, 0);

			UE_LOG(LogTemp, Warning, TEXT("[SetupInputMapping] Input Mapping Context ADDED"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
					TEXT("Input Mapping Context ADDED"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[SetupInputMapping] Input Mapping Context already exists (skipped duplicate)"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
					TEXT("Input Mapping Context already exists (skipped duplicate)"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SetupInputMapping] ERROR: InputMappingContext is NULL!"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				TEXT("ERROR: InputMappingContext is NULL!"));
		}
	}
}

void AFPSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AFPSPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void AFPSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// DEBUG: OnPossess called
	UE_LOG(LogTemp, Warning, TEXT("[OnPossess] Called for: %s | IsLocalController: %s"),
		InPawn ? *InPawn->GetName() : TEXT("NULL"),
		IsLocalController() ? TEXT("TRUE") : TEXT("FALSE"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Magenta,
			FString::Printf(TEXT("[OnPossess] Called for: %s | IsLocalController: %s"),
				InPawn ? *InPawn->GetName() : TEXT("NULL"),
				IsLocalController() ? TEXT("TRUE") : TEXT("FALSE")));
	}

	// Setup input mapping context when possessing a pawn
	SetupInputMapping();

	// Set view target to the possessed pawn (use its camera)
	if (InPawn && IsLocalController())
	{
		SetViewTarget(InPawn);

		UE_LOG(LogTemp, Display, TEXT("[OnPossess] ViewTarget set to: %s"), *InPawn->GetName());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
				FString::Printf(TEXT("ViewTarget set to: %s"), *InPawn->GetName()));
		}

		// Link animation layer LOCALLY for listen server host
		// This ensures immediate setup on the server's local player
		if (AFPSCharacter* FPSChar = Cast<AFPSCharacter>(InPawn))
		{
			FPSChar->LinkDefaultLayer();
			UE_LOG(LogTemp, Display, TEXT("[OnPossess] LinkDefaultLayer called (local controller)"));
		}
	}
}

void AFPSPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}
