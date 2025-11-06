// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "FPSCharacter.generated.h"

class UInputAction;

UENUM(BlueprintType)
enum class EFPSMovementMode : uint8
{
	Walk,
	Jog,
	Sprint,
	Crouch
};

UCLASS()
class FPSCORE_API AFPSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFPSCharacter();

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Initialize spine components from mesh bind pose
	void InitializeSpineComponents();

	// Enhanced Input callbacks
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void Move(const FInputActionValue& Value);
	void MoveCanceled(const FInputActionValue& Value);
	void WalkPressed();
	void WalkReleased();
	void SprintPressed();
	void SprintReleased();
	void CrouchPressed();
	void CrouchReleased();

	// Movement speed control
	void UpdateMovementSpeed(EFPSMovementMode NewMode);

	// Calculate sprint speed multiplier based on movement direction
	// Forward/Forward-Left/Forward-Right: 1.0 (full sprint speed)
	// Left/Right/Back/Back-Left/Back-Right: reduced sprint speed
	float GetSprintDirectionMultiplier(const FVector2D& MovementInput) const;

	// Server RPC to change movement mode
	UFUNCTION(Server, Reliable)
	void Server_SetMovementMode(EFPSMovementMode NewMode);

	// Server RPC to update pitch
	UFUNCTION(Server, Unreliable)
	void Server_UpdatePitch(float NewPitch);

	// Client RPC to setup camera, input, and hands on owning client
	// This ensures proper setup for listen server remote clients
	UFUNCTION(Client, Reliable)
	void Client_OnPossessed();

	// Helper function to update spine rotations based on current Pitch
	// Called from UpdatePitch, Server_UpdatePitch, and OnRep_Pitch
	void UpdateSpineRotations();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	// Pitch control
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void UpdatePitch(float Y);

	// Hands setup - updates first-person arms position based on active item
	// Only runs on locally controlled client (Arms mesh is OnlyOwnerSee)
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetupHandsLocation();

	// OnRep callbacks
	UFUNCTION()
	void OnRep_Pitch();

	UFUNCTION()
	void OnRep_ActiveItem();

	UFUNCTION()
	void OnRep_IsDeath();

	UFUNCTION()
	void OnRep_CurrentMovementMode();

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_Pitch, Category = "Animation")
	float Pitch = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float DeltaSeconds = 0.0f;

	// Current RAW movement input (normalized direction from player input)
	// INPUT Convention: X=right, Y=forward (W key → Y=1, D key → X=1)
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	FVector2D CurrentMovementVector = FVector2D::ZeroVector;

	// Look control
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_IsDeath, Category = "Look")
	bool bIsDeath = false;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Look")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadWrite, Category = "Look")
	bool DoingTraversalAction = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Look")
	float LookSpeed = 100.0f;

	// Active item
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_ActiveItem, Category = "Inventory")
	AActor* ActiveItem = nullptr;

	// Hands offset (LOCAL ONLY - not replicated, used for first-person arms positioning)
	// Only relevant for locally controlled player (Arms mesh is OnlyOwnerSee)
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector HandsOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	FVector DefaultHandsOffset = FVector::ZeroVector;

	// Character Movement Component (cached reference)
	UPROPERTY()
	class UCharacterMovementComponent* CMC;

	// Scene components for skeleton hierarchy
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* Spine_03;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* Spine_04;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* Spine_05;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* Neck_01;

	// Camera component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* Camera;

	// Mesh components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* Arms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* Legs;

	// Enhanced Input Actions
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Look_Yaw;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Look_Pitch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Walk;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Sprint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Crouch;

	// Movement speeds
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement")
	float JogSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement")
	float CrouchSpeed = 150.0f;

	// Current movement mode (replicated for animations)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentMovementMode, Category = "Movement")
	EFPSMovementMode CurrentMovementMode = EFPSMovementMode::Jog;
};
