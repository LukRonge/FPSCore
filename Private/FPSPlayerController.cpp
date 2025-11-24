// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSPlayerController.h"
#include "FPSCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/HUD.h"

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
	if (!IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	if (InputMappingContext)
	{
		if (!Subsystem->HasMappingContext(InputMappingContext))
		{
			Subsystem->AddMappingContext(InputMappingContext, 0);
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

	SetupInputMapping();

	if (InPawn && IsLocalController())
	{
		SetViewTarget(InPawn);

		if (AFPSCharacter* FPSChar = Cast<AFPSCharacter>(InPawn))
		{
			FPSChar->UpdateItemAnimLayer(nullptr);
		}
	}
}

void AFPSPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

// ============================================
// PLAYER HUD INTERFACE IMPLEMENTATION
// ============================================

void AFPSPlayerController::UpdateHealth_Implementation(float Health)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateHealth(HUD, Health);
	}
}

float AFPSPlayerController::GetHealth_Implementation()
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		return IPlayerHUDInterface::Execute_GetHealth(HUD);
	}

	return 0.0f;
}

void AFPSPlayerController::AddDamageEffect_Implementation()
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_AddDamageEffect(HUD);
	}
}

void AFPSPlayerController::UpdateActiveItem_Implementation(AActor* ActiveItem)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateActiveItem(HUD, ActiveItem);
	}
}

void AFPSPlayerController::UpdateInventory_Implementation(const TArray<AActor*>& Items)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateInventory(HUD, Items);
	}
}

void AFPSPlayerController::UpdateCrossHair_Implementation(bool IsAim, float LeanAlpha)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateCrossHair(HUD, IsAim, LeanAlpha);
	}
}

void AFPSPlayerController::SetCrossHair_Implementation(TSubclassOf<UUserWidget> CrossHairWidgetClass, TSubclassOf<UUserWidget> AimCrossHairWidgetClass)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_SetCrossHair(HUD, CrossHairWidgetClass, AimCrossHairWidgetClass);
	}
}

void AFPSPlayerController::SetHUDVisibility_Implementation(bool Visibility)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_SetHUDVisibility(HUD, Visibility);
	}
}

void AFPSPlayerController::UpdateItemInfo_Implementation(const FString& Info)
{
	// Delegate to HUD widget if it implements PlayerHUDInterface
	AHUD* HUD = GetHUD();
	if (HUD && HUD->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateItemInfo(HUD, Info);
	}
}
