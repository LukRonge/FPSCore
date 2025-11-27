// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/RecoilHandlerInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
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
class FPSCORE_API AFPSCharacter : public ACharacter, public IViewPointProviderInterface, public IItemCollectorInterface, public IRecoilHandlerInterface, public ICharacterMeshProviderInterface
{
	GENERATED_BODY()

public:
	AFPSCharacter();

	// IViewPointProviderInterface implementation
	virtual void GetShootingViewPoint_Implementation(FVector& OutLocation, FRotator& OutRotation) const override;
	virtual float GetViewPitch_Implementation() const override;
	virtual float GetRecoilFactor_Implementation() const override;

	// IItemCollectorInterface implementation
	virtual void Pickup_Implementation(AActor* Item) override;
	virtual void Drop_Implementation(AActor* Item) override;
	virtual AActor* GetActiveItem_Implementation() const override;

	// IRecoilHandlerInterface implementation
	virtual void ApplyRecoilKick_Implementation(float RecoilScale) override;
	virtual void ApplyCameraPitchKick_Implementation(float PitchDelta) override;
	virtual void ApplyCameraYawKick_Implementation(float YawDelta) override;
	virtual bool IsAimingDownSights_Implementation() const override;
	virtual bool IsLocalPlayer_Implementation() const override;

	// ICharacterMeshProviderInterface implementation
	virtual USkeletalMeshComponent* GetBodyMesh_Implementation() const override;
	virtual USkeletalMeshComponent* GetArmsMesh_Implementation() const override;
	virtual USkeletalMeshComponent* GetLegsMesh_Implementation() const override;

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
	void ReloadPressed();

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

	// Server RPC to set aiming state
	// Called from AimingPressed/AimingReleased to replicate bIsAiming to all clients
	UFUNCTION(Server, Reliable)
	void Server_SetAiming(bool bNewAiming);

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

	// Calculate interpolated arms offset (LOCAL ONLY - called from Tick)
	// Handles AimingAlpha interpolation, Arms position lerp, and aiming state application
	// Returns interpolated FVector between ArmsOffset (hip) and AimArmsOffset (ADS)
	FVector CalculateInterpolatedArmsOffset(float DeltaTime);

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	/**
	 * Check if character can perform reload action
	 * Validates character state for reload (no active montages, not dead, etc.)
	 * Called by ReloadComponent->CanReload_Internal()
	 *
	 * Validation checks:
	 * - No active montage in "UpperBody" slot (reload uses this slot)
	 * - Not dead (HealthComp->bIsDeath)
	 * - Future: Not on ladder (when ladder system is implemented)
	 * - Future: Not in traversal action (when traversal system is implemented)
	 *
	 * @return true if reload is possible from character state perspective
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Actions")
	bool CanReload() const;

	// ============================================
	// HEALTH COMPONENT (DAMAGE/DEATH SYSTEM)
	// ============================================

	/**
	 * Health component (manages damage, health, death STATE)
	 * Pure gameplay component with delegate-driven communication
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	class UHealthComponent* HealthComp;

	// ============================================
	// RECOIL COMPONENT (WEAPON RECOIL SYSTEM)
	// ============================================

	/**
	 * Recoil component (manages weapon recoil state, accumulation, recovery)
	 * Pure visual feedback component (LOCAL, not replicated)
	 * Handles camera kick for owning client, weapon animation for remote clients
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recoil")
	class URecoilComponent* RecoilComp;

	/**
	 * Take damage from external sources
	 * Called automatically by UGameplayStatics::ApplyDamage() or ApplyPointDamage()
	 * Delegates to HealthComponent for processing
	 *
	 * @param DamageAmount - Amount of damage to apply
	 * @param DamageEvent - Data structure with additional damage info (type, impulse, etc.)
	 * @param EventInstigator - Controller responsible for damage (for kill credit)
	 * @param DamageCauser - Actor that caused damage (weapon, projectile, etc.)
	 * @return Actual damage applied (after armor, multipliers, etc.)
	 */
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ============================================
	// HEALTH COMPONENT DELEGATE CALLBACKS
	// ============================================

	/**
	 * Called when HealthComponent broadcasts OnDamaged delegate
	 * Triggers multicast RPC for hit reaction on all clients
	 */
	UFUNCTION()
	void OnHealthComponentDamaged();

	/**
	 * Called when HealthComponent broadcasts OnDeath delegate
	 * Handles death logic (ragdoll, camera effects, respawn)
	 */
	UFUNCTION()
	void OnHealthComponentDeath();

	/**
	 * Called when HealthComponent broadcasts OnHealthChanged delegate
	 * Runs on SERVER (immediate) and CLIENTS (OnRep_Health)
	 * Updates UI (health bars) via PlayerController
	 */
	UFUNCTION()
	void OnHealthComponentHealthChanged(float NewHealth);

	// ============================================
	// HIT REACTION SYSTEM
	// ============================================

	/**
	 * Hit reaction animation montages
	 * Random montage is selected and played when character takes damage
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TArray<UAnimMontage*> HitReactionMontages;

	/**
	 * Multicast RPC for hit reaction animation/effects
	 * Called from OnHealthComponentDamaged, executes on ALL clients
	 * Plays animations + client-side screen effects
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HitReaction();

	/**
	 * Helper function to play hit reaction animation montages
	 * Called by Multicast_HitReaction on all clients
	 */
	void HitReaction();

	// ============================================
	// DEATH SYSTEM
	// ============================================

	/**
	 * Client RPC for death processing on owning client
	 * Handles client-side death effects (death camera, UI updates)
	 */
	UFUNCTION(Client, Reliable)
	void Client_ProcessDeath();

	/**
	 * Multicast RPC for death effects on all clients
	 * Handles ragdoll activation on all clients
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ProcessDeath();

	// ============================================
	// RECOIL SYSTEM
	// ============================================

	/**
	 * Multicast RPC for recoil visual feedback on all clients
	 * Called from ApplyRecoilKick_Implementation() on SERVER
	 * Executes on ALL clients (owning + remote)
	 *
	 * Owning client: Camera kick (UpdatePitch + AddControllerYawInput)
	 * Remote clients: Weapon animation (TPS mesh)
	 *
	 * @param RecoilScale - Recoil multiplier from FireComponent
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyRecoil(float RecoilScale);

	// ============================================
	// RAGDOLL SYSTEM
	// ============================================

	/**
	 * Enable ragdoll physics on character
	 * Disables movement, collision capsule, and enables physics on body mesh
	 */
	UFUNCTION(BlueprintCallable, Category = "Death")
	void EnableRagdoll();

	/**
	 * Disable ragdoll physics on character
	 * Re-enables movement, restores collision settings, and relinks animation layers
	 */
	UFUNCTION(BlueprintCallable, Category = "Death")
	void DisableRagdoll();

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
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Look")
	bool bIsAiming = false;

	// Sprint intent flag (local only, not replicated)
	// When true, character WANTS to sprint (sprint key is held)
	// Actual Sprint mode is activated only when moving forward (checked in Tick)
	bool bSprintIntentActive = false;

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

	// Current breathing scale (modified by aiming)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float CurrentBreathingScale = 1.0f;

	// Base leaning scale (hip-fire, from weapon)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float HipLeaningScale = 1.0f;

	// Aiming leaning scale (ADS, from sight)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float AimLeaningScale = 1.0f;

	// Base breathing scale (hip-fire, from weapon)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float HipBreathingScale = 1.0f;

	// Aiming breathing scale (ADS, from sight)
	UPROPERTY(BlueprintReadOnly, Category = "Look")
	float AimBreathingScale = 1.0f;

	// ============================================
	// WEAPON SWAY & LEANING SYSTEM (LOCAL ONLY)
	// ============================================

	// ============================================
	// LEANING SYSTEM (Postprocess visual offset)
	// ============================================

	// Current leaning vector (3D offset in cm): X = forward/backward, Y = lateral (left/right), Z = vertical (up/down)
	// Applied as postprocess on top of base arms position (does not affect aiming interpolation)
	UPROPERTY(BlueprintReadOnly, Category = "Leaning")
	FVector LeanVector = FVector::ZeroVector;

	// Current breathing rotation (camera sway when aiming)
	// Applied to camera rotation during ADS to simulate breathing-induced aim wobble
	UPROPERTY(BlueprintReadOnly, Category = "Leaning")
	FRotator BreathingRotation = FRotator::ZeroRotator;

	// Calculate leaning vector based on movement (velocity-based lean + perpendicular bob + vertical bob)
	// Self-contained function with hardcoded parameters and internal static state tracking
	// Returns 3D offset: X = forward offset (cm), Y = lateral offset (cm), Z = vertical offset (cm)
	UFUNCTION(BlueprintCallable, Category = "Leaning")
	FVector CalculateLeanVector(float DeltaTime);

	// Calculate breathing sway offset (idle breathing effect)
	// Self-contained function with hardcoded parameters and internal static state tracking
	// Uses CurrentBreathingScale from ActiveItem (hip) or AimBreathingScale (ADS)
	// Returns 3D offset: X = forward offset (cm), Y = lateral offset (cm), Z = vertical offset (cm)
	UFUNCTION(BlueprintCallable, Category = "Leaning")
	FVector CalculateBreathing(float DeltaTime);

	// Calculate breathing rotation for camera sway (ADS aim wobble)
	// Converts breathing offset vector to Pitch/Yaw rotation
	// Input: BreathingVector from CalculateBreathing()
	// Returns: FRotator with Pitch (vertical) and Yaw (horizontal) rotation in degrees
	UFUNCTION(BlueprintCallable, Category = "Leaning")
	FRotator CalculateBreathingRotation(const FVector& BreathingVector) const;

	// Update leaning visual feedback systems (Material Parameter Collection + Crosshair)
	// Combines LeanVector and BreathingVector to update shader effects and crosshair expansion
	// LOCAL ONLY - called from Tick() for locally controlled players
	// @param BreathingVector - Output from CalculateBreathing() for current frame
	UFUNCTION(BlueprintCallable, Category = "Leaning")
	void UpdateLeaningVisualFeedback(const FVector& BreathingVector);

private:
	// ============================================
	// MOVEMENT STATE HELPERS
	// ============================================

	// Check if character is actively sprinting (moving forward while in sprint mode)
	// Returns true if: CurrentMovementMode == Sprint AND character has velocity AND has input acceleration
	// Used to block Fire/Aim/Crouch during active sprint movement
	bool IsActivelyMoving() const;

	// ============================================
	// LEANING & BREATHING STATE TRACKING (LOCAL ONLY)
	// ============================================

	// Leaning state (replaces static TMap)
	FVector LeanState_CurrentLean = FVector::ZeroVector;
	float LeanState_WalkCycleTime = 0.0f;
	FRotator LeanState_PreviousControlRotation = FRotator::ZeroRotator;
	FVector2D LeanState_MouseLagOffset = FVector2D::ZeroVector;
	float LeanState_RawMouseDelta = 0.0f; // RAW mouse angular velocity for crosshair alpha

	// Breathing state (replaces static TMap)
	FVector BreathingState_CurrentBreathing = FVector::ZeroVector;
	float BreathingState_IdleSwayTime = 0.0f;
	float BreathingState_IdleActivation = 0.0f;

public:
	// ============================================
	// INVENTORY SYSTEM
	// ============================================
	// ARCHITECTURE NOTE:
	// - ActiveItem is the SINGLE SOURCE OF TRUTH for currently equipped item (REPLICATED)
	// - InventoryComp->Items is storage-only array (no active item tracking)
	// - External code should use IItemCollectorInterface::Execute_GetActiveItem() for access

	// Inventory component (manages item storage only, not active item)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	class UInventoryComponent* InventoryComp;

	// Currently equipped item (active in hands) - SINGLE SOURCE OF TRUTH
	// External access: IItemCollectorInterface::Execute_GetActiveItem()
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActiveItem, Category = "Inventory")
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

	// Interpolated Arms offset (result of lerp between ArmsOffset and AimArmsOffset)
	// Updated each frame in Tick via CalculateInterpolatedArmsOffset()
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	FVector InterpolatedArmsOffset = FVector::ZeroVector;

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

	// Base camera location (relative to Neck_01, set in constructor)
	// Used as reference for applying LeanVector during ADS
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	FVector BaseCameraLocation = FVector(15.0f, 0.0f, 0.0f);

	// Material Parameter Collection for weapon sway shader effects
	// Receives normalized OffsetDirection vector (leaning + breathing combined)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material")
	class UMaterialParameterCollection* MPC_Aim;

	// ============================================
	// MESH COMPONENTS
	// ============================================
	// ENCAPSULATION: External code should access via ICharacterMeshProviderInterface:
	// - ICharacterMeshProviderInterface::Execute_GetBodyMesh() for Body (GetMesh())
	// - ICharacterMeshProviderInterface::Execute_GetArmsMesh() for Arms
	// - ICharacterMeshProviderInterface::Execute_GetLegsMesh() for Legs

	// First-person arms mesh (owner-only, attached to camera)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* Arms;

	// First-person legs mesh (owner-only, attached to body)
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Reload;

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
