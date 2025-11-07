// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ControllableInterface.generated.h"

class AFPSCharacter;
class UCameraComponent;
class USpringArmComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UControllableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for controllable objects (vehicles, turrets, etc.)
 * Implemented by BaseVehicle, Turrets
 */
class FPSCORE_API IControllableInterface
{
	GENERATED_BODY()

public:
	// ============================================
	// MOVEMENT CONTROLS
	// ============================================

	// Move forward/backward
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Movement")
	void Control_MoveForward(float Value);

	// Move right/left (strafe)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Movement")
	void Control_MoveRight(float Value);

	// ============================================
	// LOOK CONTROLS
	// ============================================

	// Look up/down
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Look")
	void Control_LookUp(float Value);

	// Look right/left
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Look")
	void Control_LookRight(float Value);

	// ============================================
	// ACTIONS
	// ============================================

	// Use/interact
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Actions")
	void Control_Use();

	// Shoot/fire weapon
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Actions")
	void Control_Shoot();

	// Reload weapon
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Actions")
	void Control_Reload();

	// Jump/boost
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Actions")
	void Control_Jump();

	// ============================================
	// WEAPON SLOTS
	// ============================================

	// Switch to weapon slot 1
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Weapons")
	void Control_Weapon_1();

	// Switch to weapon slot 2
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Weapons")
	void Control_Weapon_2();

	// Switch to weapon slot 3
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Weapons")
	void Control_Weapon_3();

	// Switch to weapon slot 4
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Weapons")
	void Control_Weapon_4();

	// ============================================
	// CAMERA
	// ============================================

	// Get camera component for this controllable object
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Camera")
	UCameraComponent* GetControlledObjectCamera();

	// Get spring arm component for this controllable object
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Camera")
	USpringArmComponent* GetControlledObjectSpringArm();

	// ============================================
	// POSSESSION
	// ============================================

	// Called when character enters/possesses this object
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Possession")
	void PossessVehicle(AFPSCharacter* Character);

	// Called when character exits/unpossesses this object
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Controllable|Possession")
	void UnPossessVehicle(AFPSCharacter* Character);
};
