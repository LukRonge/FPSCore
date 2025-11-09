// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/PlayerHUDInterface.h"
#include "FPSPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class FPSCORE_API AFPSPlayerController : public APlayerController, public IPlayerHUDInterface
{
	GENERATED_BODY()

public:
	AFPSPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Setup Enhanced Input mapping context
	// Public so it can be called from Client_OnPossessed RPC
	void SetupInputMapping();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Enhanced Input Mapping Context
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* InputMappingContext;

	// ============================================
	// PLAYER HUD INTERFACE IMPLEMENTATION
	// ============================================
	// Delegates all calls to HUD widget (via GetHUD())

	virtual void UpdateHealth_Implementation(float Health) override;
	virtual float GetHealth_Implementation() override;
	virtual void AddDamageEffect_Implementation() override;
	virtual void UpdateActiveWeapon_Implementation(AActor* ActiveWeapon) override;
	virtual void UpdateCrossHair_Implementation(bool IsAim, float LeanAlpha) override;
	virtual void SetCrossHair_Implementation(TSubclassOf<UUserWidget> CrossHairWidgetClass, TSubclassOf<UUserWidget> AimCrossHairWidgetClass) override;
	virtual void SetHUDVisibility_Implementation(bool Visibility) override;
	virtual void UpdateItemInfo_Implementation(const FString& Info) override;
};
