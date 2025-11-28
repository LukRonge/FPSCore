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

	// FPS Sight component (attached to FPS mesh "attachment0" bone)
	// Visible only to owner, spawned from CurrentSightClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* FPSSightComponent;

	// TPS Sight component (attached to TPS mesh "attachment0" bone)
	// Visible to others, spawned from CurrentSightClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	UChildActorComponent* TPSSightComponent;

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

	// Current attached sight class (REPLICATED)
	// Auto-initialized in PostInitializeComponents() from DefaultSightClass
	// Can be changed at runtime for modular sight swapping
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Aiming", ReplicatedUsing = OnRep_CurrentSightClass)
	TSubclassOf<ABaseSight> CurrentSightClass;

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

	// Current attached magazine class (REPLICATED)
	// Auto-initialized in PostInitializeComponents() from DefaultMagazineClass
	// Can be changed at runtime for modular magazine swapping
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Magazine", ReplicatedUsing = OnRep_CurrentMagazineClass)
	TSubclassOf<ABaseMagazine> CurrentMagazineClass;

	// Accepted caliber type (for magazine compatibility checks)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Magazine")
	EAmmoCaliberType AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

protected:
	/**
	 * Called on CLIENTS when CurrentMagazineClass is replicated from server
	 * Re-initializes magazine components with new magazine class
	 */
	UFUNCTION()
	void OnRep_CurrentMagazineClass();

public:
	/**
	 * Initialize magazine components with specified magazine class
	 * Spawns child actor and attaches magazine meshes to weapon meshes
	 * Runs on ALL machines (called from PostInitializeComponents or OnRep)
	 * @param MagazineClass - Magazine class to spawn (nullptr to clear magazines)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Magazine")
	void InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass);

	/**
	 * Attach magazine meshes to weapon meshes
	 * LOCAL operation - each machine executes independently
	 * - FPS magazine mesh -> FPS weapon mesh (visible only to owner)
	 * - TPS magazine mesh -> TPS weapon mesh (visible to others)
	 *
	 * Called from InitMagazineComponents() after child actor is created
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

	/** Helper: Spawn muzzle flash VFX on specified mesh */
	void SpawnMuzzleFlashOnMesh(USkeletalMeshComponent* Mesh);

	/**
	 * Multicast RPC: TPSMesh effects for OTHER players
	 * - Spawns muzzle flash on TPSMesh (visible to others)
	 * - Plays shoot montage on character Body mesh (TPS view)
	 * Runs on: Server + ALL clients (but only affects TPSMesh which others see)
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMuzzleFlash();

	/**
	 * Client RPC: FPSMesh effects for OWNING CLIENT only
	 * - Spawns muzzle flash on FPSMesh (visible to owner)
	 * - Plays shoot montage on character Arms mesh (FPS view)
	 * Runs on: OWNING CLIENT only (the player who fired)
	 */
	UFUNCTION(Client, Reliable)
	void Client_PlayMuzzleFlash();

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

	// Get sight actor for transform calculations
	virtual AActor* GetSightActor_Implementation() const override;

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

private:
	// ============================================
	// HELPER METHODS
	// ============================================

	// Get current sight actor from FPSSightComponent
	// Returns nullptr if no sight attached
	AActor* GetCurrentSightActor() const;

	/**
	 * Propagate owner to all child actors (magazine, sights)
	 * Called from SetOwner() and OnRep_Owner()
	 * @param NewOwner - New owner to propagate
	 */
	void PropagateOwnerToChildActors(AActor* NewOwner);
};
