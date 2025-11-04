// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

AFPSPlayerController::AFPSPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add Input Mapping Context for Enhanced Input
	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (InputMappingContext)
			{
				Subsystem->AddMappingContext(InputMappingContext, 0);

				// DEBUG: Confirm mapping context added
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
						TEXT("Input Mapping Context ADDED"));
				}
			}
			else
			{
				// DEBUG: Mapping context is null
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
						TEXT("ERROR: InputMappingContext is NULL!"));
				}
			}
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
}

void AFPSPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}
