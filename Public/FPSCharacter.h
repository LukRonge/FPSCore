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
	void InteractPressed();
	void DropPressed();

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

	// Link default animation layer to all character meshes
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void LinkDefaultLayer();

	// Unlink default animation layer from all character meshes
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void UnlinkDefaultLayer();

	// OnRep callbacks
	UFUNCTION()
	void OnRep_Pitch();

	UFUNCTION()
	void OnRep_ActiveItem(AActor* OldActiveItem);

	// Setup active item locally (mesh attachment, animations, HUD)
	// Called from:
	// - EquipItem() on SERVER
	// - OnRep_ActiveItem() on CLIENTS
	// This follows MULTIPLAYER_GUIDELINES.md OnRep pattern
	void SetupActiveItemLocal();

	// Detach item meshes from character and re-attach to item root
	// LOCAL operation - called on both server and clients
	// Follows MULTIPLAYER_GUIDELINES.md OnRep pattern
	void DetachItemMeshes(AActor* Item);

	// Calculate drop transform and impulse for item
	// Called from DropItem() to determine where item spawns and how much force to apply
	// Can be overridden in Blueprint for custom drop behavior
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory")
	void GetDropTransformAndImpulse(AActor* Item, FTransform& OutTransform, FVector& OutImpulse);
	virtual void GetDropTransformAndImpulse_Implementation(AActor* Item, FTransform& OutTransform, FVector& OutImpulse);

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

	// ============================================
	// INVENTORY SYSTEM
	// ============================================

	// Inventory component (manages item storage)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	class UInventoryComponent* InventoryComp;

	// Currently equipped item (active in hands)
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_ActiveItem, Category = "Inventory")
	AActor* ActiveItem = nullptr;

	// Server RPC to pickup item from world
	UFUNCTION(Server, Reliable)
	void Server_PickupItem(AActor* Item);

	// Core pickup logic (SERVER ONLY - called by Server RPC)
	void PickupItem(AActor* Item);

	// Equip item from inventory into hands
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void EquipItem(AActor* Item);

	// Unequip current item (holster)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UnequipCurrentItem();

	// Switch to item at inventory index (Server RPC)
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory")
	void Server_SwitchToItemAtIndex(int32 Index);

	// Switch to item at inventory index (local execution)
	void SwitchToItemAtIndex(int32 Index);

	// Drop item from inventory back to world
	UFUNCTION(Server, Reliable)
	void Server_DropItem(AActor* Item);

	void DropItem(AActor* Item);

	// ============================================
	// DROP PARAMETERS (Blueprint Configurable)
	// ============================================

	// How far forward (in cm) from character to drop items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DropForwardDistance = 100.0f;

	// How high (in cm) from character feet to drop items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DropUpwardOffset = 50.0f;

	// Upward component of throw direction (0.0 = horizontal, 1.0 = 45° upward arc)
	// Higher values create more arcing throws
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DropUpwardArc = 0.5f;

	// Default impulse strength for dropped items (in Newtons)
	// Item can override this via IPickupableInterface::OnDropped()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DefaultDropImpulseStrength = 200.0f;

	// Hands offset (LOCAL ONLY - not replicated, used for first-person arms positioning)
	// Only relevant for locally controlled player (Arms mesh is OnlyOwnerSee)
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector HandsOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	FVector DefaultHandsOffset = FVector::ZeroVector;

	// Default animation layer class to link/unlink dynamically
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation")
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

	// Character Movement Component (cached reference)
	UPROPERTY()
	class UCharacterMovementComponent* CMC;

	// Player Controller (cached reference)
	UPROPERTY()
	class AFPSPlayerController* CachedPlayerController;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Interact;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Drop;

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

	// ============================================
	// INTERACTION SYSTEM
	// ============================================

	// Interaction trace distance from camera (in cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float InteractionDistance = 200.0f;

	// Last actor we looked at (for clearing HUD when looking away)
	UPROPERTY()
	AActor* LastInteractableActor = nullptr;

	// Perform interaction trace from camera
	// Checks for IInteractableInterface and updates PlayerController HUD
	void CheckInteractionTrace();
};
