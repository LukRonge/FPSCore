// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundCue.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/AmmoCaliberTypes.h"
#include "Interfaces/InteractableInterface.h"
#include "Interfaces/PickupableInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/SightInterface.h"
#include "BaseWeapon.generated.h"

class ABaseMagazine;
class UBallisticsComponent;
class UFireComponent;
class USightComponent;

UCLASS()
class FPSCORE_API ABaseWeapon : public AActor, public IInteractableInterface, public IPickupableInterface, public IHoldableInterface, public ISightInterface
{
	GENERATED_BODY()

public:
	ABaseWeapon();

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Override SetOwner to propagate owner to magazines
	// Called on Server + Clients (owner replicates automatically)
	virtual void SetOwner(AActor* NewOwner) override;

	// ============================================
	// COMPONENTS
	// ============================================

	// Ballistics component (pure ballistic physics and projectile spawning)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UBallisticsComponent* BallisticsComponent;

	// Fire component (fire mode mechanics: semi-auto, full-auto, burst)
	// Set in Blueprint: USemiAutoFireComponent, UFullAutoFireComponent, or UBurstFireComponent
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UFireComponent* FireComponent;

	// Sight component (handles sight configuration, aiming calculations)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	USightComponent* SightComponent;

	// FPS Magazine component (attached to FPS mesh "magazine" socket)
	// Visible only to owner, spawned from MagazineClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* FPSMagazineComponent;

	// TPS Magazine component (attached to TPS mesh "magazine" socket)
	// Visible to others, spawned from MagazineClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* TPSMagazineComponent;

	// ============================================
	// DUAL-MESH SYSTEM (FPS + TPS)
	// ============================================

	// Scene root (empty component, serves as hierarchy root)
	// Both FPS and TPS meshes are SIBLINGS (children of this root)
	// This prevents transform inheritance issues when attaching to different character sockets
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	USceneComponent* SceneRoot;

	// Third-person mesh (visible to other players, not owner)
	// Attached to SceneRoot, then to character Body mesh
	// Shadows enabled, physics collision enabled (for world pickups)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Meshes")
	USkeletalMeshComponent* TPSMesh;

	// First-person mesh (visible only to owner)
	// Attached to SceneRoot (sibling of TPSMesh), then to character Arms mesh
	// Shadows disabled, no collision (optimization)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Meshes")
	USkeletalMeshComponent* FPSMesh;

	// ============================================
	// ITEM INFO
	// ============================================

	// Weapon display name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Info")
	FText Name;

	// Weapon description
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Info", meta = (MultiLine = true))
	FText Description;

	// Additional item information
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Info", meta = (MultiLine = true))
	FText ItemInfo;

	// ============================================
	// ANIMATION
	// ============================================

	// Animation layer class for weapon-specific animations
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> AnimLayer;

	// Shooting animation montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* ShootMontage;

	// Reload animation montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* ReloadMontage;

	// ============================================
	// HANDS / IK
	// ============================================

	// Hand offset for weapon holding (location correction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hands")
	FVector HandsOffset = FVector::ZeroVector;

	// Hand offset when aiming (location correction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hands")
	FVector AimHandsOffset = FVector::ZeroVector;

	// ============================================
	// SHOOTING
	// ============================================
	// NOTE: FireRate, Spread, RecoilScale, and AcceptedCaliberType are now in BallisticsComponent
	// Configure these properties in Blueprint defaults of BallisticsComponent

	// ============================================
	// BREATHING / SWAY
	// ============================================

	// Breathing sway intensity (idle)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Breathing")
	float BreathingScale = 1.0f;

	// Breathing sway intensity when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Breathing")
	float AimBreathingScale = 0.3f;

	// ============================================
	// AIMING
	// ============================================

	// Field of view when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	float AimFOV = 50.0f;

	// Look sensitivity multiplier when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	float AimLookSpeed = 0.5f;

	// Is player currently aiming (runtime state, NOT replicated)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Aiming")
	bool IsAiming = false;

	// ============================================
	// UI
	// ============================================

	// Crosshair widget class (hip fire)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> CrossHair;

	// Crosshair widget class (aiming down sights)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> AimCrossHair;

	// ============================================
	// EFFECTS
	// ============================================

	// Muzzle flash Niagara system
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	UNiagaraSystem* MuzzleFlashNiagara;

	// ============================================
	// ATTACHMENT
	// ============================================

	// Socket name on CHARACTER where weapon mesh attaches
	// Weapon root mesh attaches to character mesh (Arms for FPS, Body for TPS) at this socket
	// Common values: "weapon_r", "hand_r", "weapon_socket"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName CharacterAttachSocket = FName("weapon_r");

	// ============================================
	// PICKUP / DROP
	// ============================================

	// Throw impulse strength when dropping weapon (applied to root mesh)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Pickup")
	float DropImpulseStrength = 200.0f;

	// ============================================
	// STATE
	// ============================================

	// Is weapon currently reloading (runtime state, NOT replicated)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|State")
	bool IsReload = false;

	// ============================================
	// MAGAZINE SYSTEM
	// ============================================

	// Current attached magazine reference (REPLICATED)
	// Auto-assigned in BeginPlay() from MagazineComponent
	// This is the authoritative magazine reference for multiplayer
	// BallisticsComponent->CurrentMagazine references this same magazine
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Magazine", ReplicatedUsing = OnRep_CurrentMagazine)
	ABaseMagazine* CurrentMagazine = nullptr;

protected:
	/**
	 * Called on CLIENTS when CurrentMagazine is replicated from server
	 * Synchronizes magazine reference to BallisticsComponent
	 */
	UFUNCTION()
	void OnRep_CurrentMagazine();

public:

	// Magazine class for auto-spawning in MagazineComponent
	// Set this in Blueprint Class Defaults (e.g., BP_Magazine_AK47)
	// Magazine component will automatically spawn this class in BeginPlay()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Magazine")
	TSubclassOf<ABaseMagazine> MagazineClass;

	// Accepted caliber type (for magazine compatibility checks)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Magazine")
	EAmmoCaliberType AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

	// ============================================
	// FIRE API (Called by FPSCharacter)
	// ============================================

	/**
	 * Trigger shoot (fire button pressed)
	 * Called by FPSCharacter when IA_Shoot input Started/Ongoing
	 * Delegates to FireComponent->TriggerPulled()
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Fire")
	void TriggerShoot();

	/**
	 * Trigger release (fire button released)
	 * Called by FPSCharacter when IA_Shoot input Completed/Canceled
	 * Delegates to FireComponent->TriggerReleased()
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Fire")
	void TriggerRelease();

	// ============================================
	// INTERACTABLE INTERFACE
	// ============================================

	// Get available interaction verbs
	virtual void GetVerbs_Implementation(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const override;

	// Check if interaction is allowed (only if weapon has no owner)
	virtual bool CanInteract_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const override;

	// Perform interaction
	virtual void Interact_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) override;

	// Get interaction text for UI
	virtual FText GetInteractionText_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const override;

	// ============================================
	// PICKUPABLE INTERFACE
	// ============================================

	// Check if weapon can be picked up
	virtual bool CanBePicked_Implementation(const FInteractionContext& Ctx) const override;

	// Called when weapon is picked up (SERVER ONLY)
	virtual void OnPicked_Implementation(APawn* Picker, const FInteractionContext& Ctx) override;

	// Called when weapon is dropped back to world (SERVER ONLY)
	virtual void OnDropped_Implementation(const FInteractionContext& Ctx) override;

	// ============================================
	// HOLDABLE INTERFACE
	// ============================================

	// Called when weapon is equipped into character's hands
	virtual void OnEquipped_Implementation(APawn* Owner) override;

	// Called when weapon is unequipped
	virtual void OnUnequipped_Implementation() override;

	// Check if weapon is currently equipped
	virtual bool IsEquipped_Implementation() const override;

	// Get hands offset for FPS arms positioning
	virtual FVector GetHandsOffset_Implementation() const override;

	// Get FPS mesh component (visible only to owner)
	virtual UMeshComponent* GetFPSMeshComponent_Implementation() const override;

	// Get TPS mesh component (visible to others, not owner)
	virtual UMeshComponent* GetTPSMeshComponent_Implementation() const override;

	// Get attachment socket name
	virtual FName GetAttachSocket_Implementation() const override;

	// Get animation layer class
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const override;

	// ============================================
	// UTILITY METHODS
	// ============================================

	// Get weapon skeletal mesh component (TPS mesh, root component)
	// DEPRECATED: Use GetTPSMesh() instead for clarity
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh")
	USkeletalMeshComponent* GetWeaponMesh() const { return TPSMesh; }

	// Get FPS mesh component (visible only to owner)
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh")
	USkeletalMeshComponent* GetFPSMesh() const { return FPSMesh; }

	// Get TPS mesh component (root, visible to others)
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh")
	USkeletalMeshComponent* GetTPSMesh() const { return TPSMesh; }

	// Get aim hands offset (when aiming down sights)
	UFUNCTION(BlueprintPure, Category = "Weapon|Hands")
	FVector GetAimHandsOffset() const { return AimHandsOffset; }
};
