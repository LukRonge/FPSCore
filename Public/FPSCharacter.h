// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/ItemCollectorInterface.h"
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
class FPSCORE_API AFPSCharacter : public ACharacter, public IViewPointProviderInterface, public IItemCollectorInterface
{
	GENERATED_BODY()

public:
	AFPSCharacter();

	// IViewPointProviderInterface implementation
	virtual void GetShootingViewPoint_Implementation(FVector& OutLocation, FRotator& OutRotation) const override;
	virtual float GetViewPitch_Implementation() const override;

	// IItemCollectorInterface implementation
	virtual void Pickup_Implementation(AActor* Item) override;
	virtual void Drop_Implementation(AActor* Item) override;

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
	void JumpPressed();
	void JumpReleased();
	void InteractPressed();
	void DropPressed();
	void UseStarted();
	void UseStopped();
	void AimingPressed();
	void AimingReleased();

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

	// Calculate network pitch from actual camera direction (inverse calculation)
	// This ensures ViewPointProvider returns the same direction as local camera
	// Returns pitch in degrees that should be replicated for correct weapon ballistics
	float CalculateNetworkPitchFromCamera() const;

	// Update aiming interpolation (LOCAL ONLY - called from Tick)
	// Handles AimingAlpha interpolation, Arms position lerp, and aiming state application
	void UpdateAimingInterpolation(float DeltaTime);

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
	void SetupArmsLocation(AActor* Item = nullptr);

	// Update animation layer based on item (item anim layer if valid, default layer if null)
	// @param Item - Item to get anim layer from (nullptr = switch to default layer)
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void UpdateItemAnimLayer(AActor* Item);

	// OnRep callbacks
	UFUNCTION()
	void OnRep_Pitch();

	UFUNCTION()
	void OnRep_ActiveItem(AActor* OldActiveItem);

	UFUNCTION()
	void OnRep_IsDeath();

	UFUNCTION()
	void OnRep_CurrentMovementMode();

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_Pitch, Category = "Animation")
	float Pitch = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float DeltaSeconds = 0.0f;

	// Local pitch accumulator for spine rotations (NOT replicated)
	// This is the raw input-driven pitch for visual spine animations
	// The replicated Pitch variable is calculated from actual camera direction
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float LocalPitchAccumulator = 0.0f;

	// Debug draw camera vs network pitch direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDrawPitchDirections = false;

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

	// Current look speed multiplier (modified by aiming)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float CurrentLookSpeed = 1.0f;

	// Current leaning scale (modified by aiming)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float CurrentLeaningScale = 1.0f;

	// ============================================
	// WEAPON SWAY & LEANING SYSTEM (LOCAL ONLY)
	// ============================================

	// ============================================
	// LEANING SYSTEM (Postprocess visual offset)
	// ============================================

	// Current leaning vector (2D offset in cm): X = lateral (left/right), Y = forward/backward
	// Applied as postprocess on top of base arms position (does not affect aiming interpolation)
	UPROPERTY(BlueprintReadOnly, Category = "Leaning")
	FVector2D LeanVector = FVector2D::ZeroVector;

	// Calculate leaning vector based on movement (velocity-based lean + perpendicular bob)
	// Self-contained function with hardcoded parameters and internal static state tracking
	// Returns 2D offset: X = lateral offset (cm), Y = forward offset (cm)
	UFUNCTION(BlueprintCallable, Category = "Leaning")
	FVector2D CalculateLeanVector(float DeltaTime);

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

	// Multicast RPC for physical pickup setup (runs on ALL clients)
	// Disables physics, attaches to character body, hides item
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PickupItem(AActor* Item);

	// Physical pickup implementation (called by Multicast)
	void PerformPickup(AActor* Item);

	// Server RPC to drop item from inventory
	UFUNCTION(Server, Reliable)
	void Server_DropItem(AActor* Item);

	// Multicast RPC for physical drop setup (runs on ALL clients)
	// Detaches from character, enables physics, places in world
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DropItem(AActor* Item);

	// Physical drop implementation (called by Multicast)
	void PerformDrop(AActor* Item);

	// Calculate drop transform and physics impulse for dropped items
	// Used by DropItem() to spawn item in front of character with throw arc
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory")
	void GetDropTransformAndImpulse(AActor* Item, FTransform& OutTransform, FVector& OutImpulse);

	// ============================================
	// EQUIP/UNEQUIP SYSTEM
	// ============================================

	// Equip item from inventory (SERVER ONLY)
	void EquipItem(AActor* Item);

	// Unequip item (SERVER ONLY)
	void UnEquipItem(AActor* Item);

private:
	// Setup active item local visual state (LOCAL operation - runs on ALL machines)
	void SetupActiveItemLocal();

public:

	// ============================================
	// INVENTORY EVENT CALLBACKS
	// ============================================

	// Called when item is added to inventory (bound to InventoryComp->OnItemAdded)
	UFUNCTION()
	void OnInventoryItemAdded(AActor* Item);

	// Called when item is removed from inventory (bound to InventoryComp->OnItemRemoved)
	UFUNCTION()
	void OnInventoryItemRemoved(AActor* Item);

	// Called when inventory is cleared (bound to InventoryComp->OnInventoryCleared)
	UFUNCTION()
	void OnInventoryCleared();

	// Hands offset (LOCAL ONLY - not replicated, used for first-person arms positioning)
	// Only relevant for locally controlled player (Arms mesh is OnlyOwnerSee)
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector ArmsOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector AimArmsOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	FVector DefaultArmsOffset = FVector::ZeroVector;

	// ============================================
	// AIMING INTERPOLATION (LOCAL ONLY)
	// ============================================

	// Aiming interpolation speed (constant)
	const float AimingInterpSpeed = 30.0f;

	// Current aiming interpolation alpha (0.0 = hip, 1.0 = ADS)
	// LOCAL ONLY - not replicated, visual interpolation only
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float AimingAlpha = 0.0f;

	// Flag to track if aiming state (FOV, crosshair, look speed) has been applied
	// Prevents repeated calls when AimingAlpha > 0.8
	// LOCAL ONLY - not replicated
	bool bAimingCrosshairSet = false;

	// Note: bIsAiming already exists at line 157 (replicated, used for look sensitivity)

	// Default animation layer class to link/unlink dynamically
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation")
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

	// Default crosshair widget class (shown when no active item)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> DefaultCrossHair;

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

	// Original Spine_03 location (for crouch offset)
	FVector OriginalSpineLocation = FVector::ZeroVector;

	// Camera component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* Camera;

	// Default camera FOV (non-aiming)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float DefaultFOV = 90.0f;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Shoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Jump;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Aim;

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

	// ============================================
	// DROP PARAMETERS
	// ============================================

	// Forward distance from camera viewpoint to drop item (in cm)
	// Recommended: ~30cm to spawn outside capsule collision
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DropForwardDistance = 30.0f;

	// Vertical offset from camera viewpoint (in cm)
	// Negative value = below camera (hand level), Positive = above camera
	// Recommended: -20cm for hand level drop point
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DropUpwardOffset = -20.0f;

	// Upward arc component for throw direction (0 = horizontal, 1 = 45° upward)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DropUpwardArc = 0.5f;

	// Base impulse strength for thrown items (in cm/s when bVelChange=true)
	// Recommended: 400-600 for 2m throw distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float DefaultDropImpulseStrength = 200.0f;

	// Multiplier for character velocity inheritance (0 = no inheritance, 1 = full inheritance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drop")
	float VelocityInheritanceMultiplier = 1.0f;

	// ============================================
	// INTERACTION PARAMETERS
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
