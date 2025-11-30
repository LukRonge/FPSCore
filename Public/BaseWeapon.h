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
#include "Interfaces/ReloadableInterface.h"
#include "Interfaces/ItemWidgetProviderInterface.h"
#include "BaseWeapon.generated.h"

class ABaseMagazine;
class UBallisticsComponent;
class UFireComponent;
class UReloadComponent;
class ABaseSight;

UCLASS()
class FPSCORE_API ABaseWeapon : public AActor, public IInteractableInterface, public IPickupableInterface, public IHoldableInterface, public ISightInterface, public IUsableInterface, public IAmmoConsumerInterface, public IBallisticsHandlerInterface, public IReloadableInterface, public IItemWidgetProviderInterface
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

	/**
	 * Called on CLIENTS when Owner property is replicated from server
	 * Propagates owner to child actors (magazines, sights) to fix visibility
	 *
	 * CRITICAL: Without this override, remote clients won't have correct
	 * magazine/sight visibility because SetOwner() cascade only happens
	 * on server when weapon is picked up
	 */
	virtual void OnRep_Owner() override;

protected:
	// ============================================
	// COMPONENTS (PROTECTED - use interfaces for external access)
	// ============================================
	// ENCAPSULATION: External code should access via interfaces:
	// - IReloadableInterface::GetReloadComponent()
	// - IReloadableInterface::GetMagazineActor()
	// - IReloadableInterface::GetFPSMagazineMesh() / GetTPSMagazineMesh()
	// - IHoldableInterface::GetFPSMeshComponent() / GetTPSMeshComponent()

	// Ballistics component (pure ballistic physics and projectile spawning)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UBallisticsComponent* BallisticsComponent;

	// Fire component (fire mode mechanics: semi-auto, full-auto, burst)
	// Set in Blueprint: USemiAutoFireComponent, UFullAutoFireComponent, or UBurstFireComponent
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Components")
	UFireComponent* FireComponent;

	// Reload component (reload logic and ammo transfer)
	// Set in Blueprint: UBoxMagazineReloadComponent, UBoltActionReloadComponent, etc.
	// External access: IReloadableInterface::Execute_GetReloadComponent()
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Components")
	UReloadComponent* ReloadComponent = nullptr;

	// Magazine component (single ChildActorComponent attached to TPSMesh "magazine" socket)
	// Magazine actor has its own FPS/TPS meshes with appropriate visibility
	// External access: IReloadableInterface::Execute_GetMagazineActor()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* MagazineComponent;

	// Sight component (single ChildActorComponent attached to FPSMesh)
	// Socket name obtained from sight via ISightMeshProviderInterface::GetAttachSocket()
	// Sight FPS mesh attaches to weapon FPSMesh, TPS mesh attaches to weapon TPSMesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* SightComponent;

protected:
	// ============================================
	// DUAL-MESH SYSTEM (FPS + TPS)
	// ============================================
	// ENCAPSULATION: External code should access via interfaces:
	// - IHoldableInterface::GetFPSMeshComponent()
	// - IHoldableInterface::GetTPSMeshComponent()

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

public:

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
	// IMPORTANT: Shoot montages use slot "DefaultGroup.Shoot"
	// This allows shooting and reloading to coexist (Reload uses "DefaultGroup.UpperBody")
	// Played on Body, Arms, and Legs meshes via Multicast_PlayMuzzleFlash()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* ShootMontage;

	// NOTE: Reload montages moved to ReloadComponent (Body/Arms/Legs separation)
	// Configure reload animations in ReloadComponent Blueprint defaults

	// ============================================
	// EQUIP/UNEQUIP ANIMATION
	// ============================================

	// Equip animation montage (draw weapon, unfold launcher, etc.)
	// Played on character Body/Arms/Legs meshes when weapon is equipped
	// Item is attached to weapon_r during montage, ready to use after completion
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* EquipMontage;

	// Unequip animation montage (holster weapon, fold launcher, etc.)
	// Played on character Body/Arms/Legs meshes when weapon is unequipped
	// Item is moved to spine_03 and hidden after montage completion
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	UAnimMontage* UnequipMontage;

	// Current equipping state (montage playing)
	// NOT replicated - visual state only, derived from montage playback
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsEquipping = false;

	// Current unequipping state (montage playing)
	// NOT replicated - visual state only, derived from montage playback
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsUnequipping = false;

	// ============================================
	// HANDS / IK
	// ============================================

	// Arms offset for weapon holding (location correction)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Arms")
	FVector ArmsOffset = FVector::ZeroVector;

	// ============================================
	// SHOOTING
	// ============================================
	// NOTE: FireRate, Spread, RecoilScale, and AcceptedCaliberType are now in BallisticsComponent
	// Configure these properties in Blueprint defaults of BallisticsComponent

	// ============================================
	// BREATHING / SWAY
	// ============================================

	// Breathing sway intensity (hip-fire idle)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Breathing")
	float BreathingScale = 1.0f;

	// ============================================
	// AIMING
	// ============================================

	// Field of view when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	float AimFOV = 50.0f;

	// Look sensitivity multiplier when aiming
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aiming")
	float AimLookSpeed = 0.5f;

	// Leaning scale multiplier when holding this weapon (1.0 = normal scale)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Movement")
	float LeaningScale = 1.0f;

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

	// Current attached sight actor reference (REPLICATED)
	// SERVER: Set in InitSightComponents() after child actor creation
	// CLIENT: Receives via replication, OnRep handles attachment
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Aiming", ReplicatedUsing = OnRep_CurrentSight)
	ABaseSight* CurrentSight = nullptr;

	// Hip-fire crosshair widget class (shown when not aiming)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> CrossHair;

	// Item widget class for HUD display (bottom-right corner)
	// Widget shows weapon info: ammo count, magazine capacity
	// Widget pulls data from weapon via IAmmoConsumerInterface
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> ItemWidgetClass;

protected:
	/**
	 * Called on CLIENTS when CurrentSight actor is replicated from server
	 * Attaches sight meshes to weapon meshes and propagates owner
	 */
	UFUNCTION()
	void OnRep_CurrentSight();

public:
	/**
	 * Initialize sight component with specified sight class (SERVER ONLY)
	 * Spawns child actor, sets CurrentSight, and attaches meshes
	 * Client receives CurrentSight via replication → OnRep_CurrentSight handles attachment
	 * @param SightClass - Sight class to spawn (nullptr to clear sight)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
	void InitSightComponents(TSubclassOf<ABaseSight> SightClass);

	/**
	 * Attach sight meshes to weapon meshes
	 * LOCAL operation - each machine executes independently
	 * - FPS sight mesh -> FPS weapon mesh (visible only to owner)
	 * - TPS sight mesh -> TPS weapon mesh (visible to others)
	 *
	 * Called from InitSightComponents() (server) or OnRep_CurrentSight() (client)
	 */
	void AttachSightMeshes();

	// ============================================
	// EFFECTS
	// ============================================

	// Muzzle flash Niagara system
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	UNiagaraSystem* MuzzleFlashNiagara;

	// ============================================
	// ATTACHMENT
	// ============================================

	// Socket name on CHARACTER where weapon mesh attaches when equipped (default position)
	// Weapon root mesh attaches to character mesh (Arms for FPS, Body for TPS) at this socket
	// Common values: "weapon_r", "hand_r", "weapon_socket"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName CharacterAttachSocket = FName("weapon_r");

	// Socket name on CHARACTER where weapon mesh attaches during reload/bolt-action operations
	// Used by bolt-action rifles, shotguns, and other weapons that require hand repositioning
	// The weapon re-attaches to this socket during reload, then back to CharacterAttachSocket after
	// Common values: "weapon_r" (same as equip), "weapon_l" (left hand for bolt-action)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName ReloadAttachSocket = FName("weapon_r");

	// ============================================
	// PICKUP / DROP
	// ============================================

	// Throw impulse strength when dropping weapon (applied to root mesh)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Pickup")
	float DropImpulseStrength = 200.0f;

	// ============================================
	// STATE
	// ============================================

	// NOTE: Reload state moved to ReloadComponent->bIsReloading (REPLICATED)
	// Use IReloadableInterface::IsReloading() to check reload state

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

	// Default magazine class (set in Blueprint Class Defaults)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Magazine")
	TSubclassOf<ABaseMagazine> DefaultMagazineClass;

	// Accepted caliber type (for magazine compatibility checks)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Magazine")
	EAmmoCaliberType AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

public:
	/**
	 * Initialize magazine component with specified magazine class (SERVER ONLY)
	 * Spawns child actor, sets CurrentMagazine, and attaches meshes
	 * Client receives CurrentMagazine via replication → OnRep_CurrentMagazine handles attachment
	 * @param MagazineClass - Magazine class to spawn (nullptr to clear magazine)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Magazine")
	void InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass);

	/**
	 * Attach magazine meshes to weapon meshes
	 * LOCAL operation - each machine executes independently
	 * - FPS magazine mesh -> FPS weapon mesh (visible only to owner)
	 * - TPS magazine mesh -> TPS weapon mesh (visible to others)
	 *
	 * Called from InitMagazineComponents() (server) or OnRep_CurrentMagazine() (client)
	 */
	void AttachMagazineMeshes();

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
	//
	// ARCHITECTURE:
	// - Multicast_PlayMuzzleFlash: TPSMesh effects (visible to OTHER players)
	// - Client_PlayMuzzleFlash: FPSMesh effects (visible ONLY to owning client)
	//
	// Server calls BOTH RPCs:
	// 1. Multicast → TPSMesh muzzle VFX + character TPS shoot anims (others see)
	// 2. Client → FPSMesh muzzle VFX + character FPS shoot anims (owner sees)
	//
	// This follows multiplayer guidelines:
	// - FPSMesh has SetOnlyOwnerSee(true) → only owning client sees it
	// - TPSMesh has SetOwnerNoSee(true) → only other clients see it
	//
	// NOTE: No parameters needed - VFX spawns on mesh bone socket "barrel"
	// ============================================

protected:

	/**
	 * Helper: Spawn muzzle flash VFX on specified mesh
	 * @param Mesh - Mesh to attach Niagara to
	 * @param bIsFirstPerson - True for FPS mesh (OnlyOwnerSee), False for TPS mesh (OwnerNoSee)
	 */
	void SpawnMuzzleFlashOnMesh(USkeletalMeshComponent* Mesh, bool bIsFirstPerson);

	/**
	 * Single Multicast RPC for ALL shoot visual effects
	 * Each machine locally determines what to render:
	 * - IsLocallyControlled() → FPS view (FPSMesh VFX + Arms animation)
	 * - !IsLocallyControlled() → TPS view (TPSMesh VFX + Body/Legs animation)
	 * - DedicatedServer → Skip VFX, keep animations for physics
	 *
	 * ARCHITECTURE: Replicate the EVENT, not the visuals
	 * Each client spawns appropriate effects based on their perspective
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayShootEffects();

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

	// Get arms offset for FPS arms positioning
	virtual FVector GetArmsOffset_Implementation() const override;

	// Get FPS mesh component (visible only to owner)
	virtual UPrimitiveComponent* GetFPSMeshComponent_Implementation() const override;

	// Get TPS mesh component (visible to others, not owner)
	virtual UPrimitiveComponent* GetTPSMeshComponent_Implementation() const override;

	// Get attachment socket name
	virtual FName GetAttachSocket_Implementation() const override;

	// Get reload attachment socket name
	virtual FName GetReloadAttachSocket_Implementation() const override;

	// Get animation layer class
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const override;

	// Get leaning scale
	virtual float GetLeaningScale_Implementation() const override;

	// Get breathing scale
	virtual float GetBreathingScale_Implementation() const override;

	// Set FPS mesh visibility
	virtual void SetFPSMeshVisibility_Implementation(bool bVisible) override;

	// Set aiming state (called by FPSCharacter)
	virtual void SetAiming_Implementation(bool bAiming) override;

	// Get current aiming state
	virtual bool GetIsAiming_Implementation() const override;

	/**
	 * Check if weapon allows aiming right now
	 * Returns false if busy (reloading, etc.)
	 *
	 * @return true if aiming is allowed, false if blocked
	 */
	virtual bool CanAim_Implementation() const override;

	/**
	 * Check if weapon can be unequipped/dropped right now
	 * Returns false if busy (reloading, playing montage, etc.)
	 *
	 * Checks:
	 * - ReloadComponent->bIsReloading
	 * - FPSMesh AnimInstance montage playing
	 * - bIsEquipping or bIsUnequipping
	 *
	 * @return true if weapon can be unequipped, false if blocked
	 */
	virtual bool CanBeUnequipped_Implementation() const override;

	// Get equip montage
	virtual UAnimMontage* GetEquipMontage_Implementation() const override;

	// Get unequip montage
	virtual UAnimMontage* GetUnequipMontage_Implementation() const override;

	// Check if currently equipping
	virtual bool IsEquipping_Implementation() const override;

	// Check if currently unequipping
	virtual bool IsUnequipping_Implementation() const override;

	// Set equipping state
	virtual void SetEquippingState_Implementation(bool bEquipping) override;

	// Set unequipping state
	virtual void SetUnequippingState_Implementation(bool bUnequipping) override;

	// Called when equip montage completes
	virtual void OnEquipMontageComplete_Implementation(APawn* Owner) override;

	// Called when unequip montage completes
	virtual void OnUnequipMontageComplete_Implementation(APawn* Owner) override;

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

	/**
	 * Get aim transform in world space
	 * Delegates to CurrentSight if available, otherwise uses FPSMesh "aim" socket
	 * Returns invalid transform (scale=0) if aiming not possible
	 */
	virtual FTransform GetAimTransform_Implementation() const override;

	// Get aiming FOV from current sight or default weapon AimFOV
	virtual float GetAimingFOV_Implementation() const override;

	// Get aim look speed from current sight or default
	virtual float GetAimLookSpeed_Implementation() const override;

	// Get aim leaning scale from current sight or default
	virtual float GetAimLeaningScale_Implementation() const override;

	// Get aim breathing scale from current sight or default
	virtual float GetAimBreathingScale_Implementation() const override;

	// Check if current sight wants to hide FPS mesh when aiming
	virtual bool ShouldHideFPSMeshWhenAiming_Implementation() const override;

	// ============================================
	// RELOADABLE INTERFACE
	// ============================================

	/**
	 * Check if weapon can be reloaded right now
	 * Delegates to ReloadComponent->CanReload_Internal()
	 * @return true if reload is possible (magazine not full, not already reloading, etc.)
	 */
	virtual bool CanReload_Implementation() const override;

	/**
	 * Start reload sequence (triggers SERVER RPC)
	 * Delegates to ReloadComponent->Server_StartReload()
	 * @param Ctx - Use context with controller, pawn, aim data
	 */
	virtual void Reload_Implementation(const FUseContext& Ctx) override;

	/**
	 * Check if weapon is currently reloading
	 * Delegates to ReloadComponent->bIsReloading
	 * @return true if reload is in progress
	 */
	virtual bool IsReloading_Implementation() const override;

	/**
	 * Get magazine actor (single authoritative magazine)
	 * @return Magazine actor or nullptr if not available
	 */
	virtual AActor* GetMagazineActor_Implementation() const override;

	/**
	 * Get FPS magazine mesh (visible only to owner)
	 * Delegates to magazine actor via IReloadableInterface
	 * @return FPS mesh component or nullptr
	 */
	virtual UPrimitiveComponent* GetFPSMagazineMesh_Implementation() const override;

	/**
	 * Get TPS magazine mesh (visible to others, not owner)
	 * Delegates to magazine actor via IReloadableInterface
	 * @return TPS mesh component or nullptr
	 */
	virtual UPrimitiveComponent* GetTPSMagazineMesh_Implementation() const override;

	/**
	 * Get ReloadComponent from this weapon
	 * @return ReloadComponent or nullptr if not available
	 */
	virtual UReloadComponent* GetReloadComponent_Implementation() const override;

	/**
	 * Called when reload completes successfully
	 * Override in child classes for weapon-specific reload completion behavior
	 * Example: M4A1 resets bBoltCarrierOpen = false
	 *
	 * Called from ReloadComponent::OnReloadComplete() via IReloadableInterface
	 */
	virtual void OnWeaponReloadComplete_Implementation() override;

	// ============================================
	// ITEM WIDGET PROVIDER INTERFACE
	// ============================================

	// Get item widget class for HUD display
	virtual TSubclassOf<UUserWidget> GetItemWidgetClass_Implementation() const override;

	// ============================================
	// UTILITY METHODS
	// ============================================

	// Get FPS mesh component (visible only to owner)
	// NOTE: Prefer IHoldableInterface::GetFPSMeshComponent() for external access
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh")
	USkeletalMeshComponent* GetFPSMesh() const { return FPSMesh; }

	// Get TPS mesh component (visible to others)
	// NOTE: Prefer IHoldableInterface::GetTPSMeshComponent() for external access
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh")
	USkeletalMeshComponent* GetTPSMesh() const { return TPSMesh; }

protected:
	// ============================================
	// HELPER METHODS FOR CHILD CLASSES
	// ============================================

	/**
	 * Play montage on weapon meshes (FPSMesh + TPSMesh)
	 * Uses IHoldableInterface to access meshes (Golden Rule compliance)
	 * Used for weapon-specific animations (bolt carrier, slide, etc.)
	 * @param Montage - Animation montage to play on both meshes
	 */
	void PlayWeaponMontage(UAnimMontage* Montage);

	/**
	 * Force update AnimInstances on weapon meshes
	 * Call this after changing replicated state variables that AnimBP reads
	 * Uses IHoldableInterface to access meshes (Golden Rule compliance)
	 */
	void ForceUpdateWeaponAnimInstances();

private:
	// ============================================
	// PRIVATE HELPER METHODS
	// ============================================

	/**
	 * Propagate owner to all child actors (magazine, sights)
	 * Called from SetOwner() and OnRep_Owner()
	 * @param NewOwner - New owner to propagate
	 */
	void PropagateOwnerToChildActors(AActor* NewOwner);
};
