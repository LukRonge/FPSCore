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
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/SightInterface.h"
#include "Interfaces/UsableInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/BallisticsHandlerInterface.h"
#include "BaseWeapon.generated.h"

class ABaseMagazine;
class UBallisticsComponent;
class UFireComponent;
class ABaseSight;

UCLASS()
class FPSCORE_API ABaseWeapon : public AActor, public IInteractableInterface, public IPickupableInterface, public IHoldableInterface, public ISightInterface, public IUsableInterface, public IAmmoConsumerInterface, public IBallisticsHandlerInterface
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
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Components")
	UFireComponent* FireComponent;

	// FPS Magazine component (attached to FPS mesh "magazine" socket)
	// Visible only to owner, spawned from MagazineClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* FPSMagazineComponent;

	// TPS Magazine component (attached to TPS mesh "magazine" socket)
	// Visible to others, spawned from MagazineClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* TPSMagazineComponent;

	// FPS Sight component (attached to FPS mesh "attachment_body" bone)
	// Visible only to owner, spawned from CurrentSightClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* FPSSightComponent;

	// TPS Sight component (attached to TPS mesh "attachment_body" bone)
	// Visible to others, spawned from CurrentSightClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* TPSSightComponent;

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

	// Default aiming point for camera positioning when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	FVector DefaultAimPoint = FVector::ZeroVector;

	// Default sight class (iron sights, red dot, scope, etc.)
	// Set in Blueprint Class Defaults
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	TSubclassOf<ABaseSight> DefaultSightClass;

	// Current attached sight class (REPLICATED)
	// Auto-initialized in PostInitializeComponents() from DefaultSightClass
	// Can be changed at runtime for modular sight swapping
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Aiming", ReplicatedUsing = OnRep_CurrentSightClass)
	TSubclassOf<ABaseSight> CurrentSightClass;

	// Hip-fire crosshair widget class (shown when not aiming)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> CrossHair;

protected:
	/**
	 * Called on CLIENTS when CurrentSightClass is replicated from server
	 * Re-initializes sight components with new sight class
	 */
	UFUNCTION()
	void OnRep_CurrentSightClass();

public:
	/**
	 * Initialize sight components with specified sight class
	 * Spawns child actors for FPSSightComponent and TPSSightComponent
	 * Runs on ALL machines (called from PostInitializeComponents or OnRep)
	 * @param SightClass - Sight class to spawn (nullptr to clear sights)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
	void InitSightComponents(TSubclassOf<ABaseSight> SightClass);

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
	// USABLE INTERFACE (Item usage - shoot, aim, etc.)
	// ============================================

	// Called when use input STARTED (IA_Use pressed - shoot started)
	virtual void UseStart_Implementation(const FUseContext& Ctx) override;

	// Called every frame while use input HELD (optional - for continuous actions)
	virtual void UseTick_Implementation(const FUseContext& Ctx) override;

	// Called when use input STOPPED (IA_Use released - shoot stopped)
	virtual void UseStop_Implementation(const FUseContext& Ctx) override;

	// Check if weapon is currently being used
	virtual bool IsUsing_Implementation() const override;

	// ============================================
	// BALLISTICS HANDLER INTERFACE
	// ============================================

	/**
	 * Handle shot fired event (SERVER ONLY)
	 * Called by BallisticsComponent when shot is fired
	 * Triggers Multicast RPC to spawn muzzle flash on all clients
	 */
	virtual void HandleShotFired_Implementation(
		FVector_NetQuantize MuzzleLocation,
		FVector_NetQuantizeNormal Direction
	) override;

	/**
	 * Handle impact detected event (SERVER ONLY)
	 * Called by BallisticsComponent when impact is detected
	 * Triggers Multicast RPC to spawn impact effects on all clients
	 */
	virtual void HandleImpactDetected_Implementation(
		const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX,
		FVector_NetQuantize Location,
		FVector_NetQuantizeNormal Normal
	) override;

	// ============================================
	// MUZZLE EFFECTS (Multiplayer)
	// ============================================

protected:

	/**
	 * Multicast RPC for spawning muzzle flash on all clients
	 * Spawns Niagara muzzle flash, plays sound, animates weapon
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMuzzleFlash(
		FVector_NetQuantize MuzzleLocation,
		FVector_NetQuantizeNormal Direction
	);

	/**
	 * Multicast RPC for spawning impact effects on all clients
	 * Spawns Niagara system (includes particles, sound, decals)
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnImpactEffect(
		const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX,
		FVector_NetQuantize Location,
		FVector_NetQuantizeNormal Normal
	);

public:
	// ============================================
	// SERVER RPC (Multiplayer)
	// ============================================

	// Server RPC for shoot action (true = pressed, false = released)
	UFUNCTION(Server, Reliable)
	void Server_Shoot(bool bPressed);

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
	virtual UPrimitiveComponent* GetFPSMeshComponent_Implementation() const override;

	// Get TPS mesh component (visible to others, not owner)
	virtual UPrimitiveComponent* GetTPSMeshComponent_Implementation() const override;

	// Get attachment socket name
	virtual FName GetAttachSocket_Implementation() const override;

	// Get animation layer class
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const override;

	// Set FPS mesh visibility
	virtual void SetFPSMeshVisibility_Implementation(bool bVisible) override;

	// ============================================
	// AMMO CONSUMER INTERFACE
	// ============================================

	// Get ammo type from magazine
	virtual FName GetAmmoType_Implementation() const override;

	// Get current ammo in magazine
	virtual int32 GetClip_Implementation() const override;

	// Get max magazine capacity
	virtual int32 GetClipSize_Implementation() const override;

	// Get total reserve ammo (0 for now - no reserve system yet)
	virtual int32 GetTotalAmmo_Implementation() const override;

	// Consume ammo from magazine (SERVER ONLY)
	virtual int32 ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx) override;

	// ============================================
	// SIGHT INTERFACE
	// ============================================

	// Get hip-fire crosshair from weapon
	virtual TSubclassOf<UUserWidget> GetCrossHair_Implementation() const override;

	// Get aiming crosshair from current sight
	virtual TSubclassOf<UUserWidget> GetAimingCrosshair_Implementation() const override;

	// Get aiming point from current sight or default
	virtual FVector GetAimingPoint_Implementation() const override;

	// Check if current sight wants to hide FPS mesh when aiming
	virtual bool ShouldHideFPSMeshWhenAiming_Implementation() const override;

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
