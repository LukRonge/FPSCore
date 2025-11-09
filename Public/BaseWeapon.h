// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
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
#include "BaseWeapon.generated.h"

class ABaseMagazine;

UCLASS()
class FPSCORE_API ABaseWeapon : public AActor, public IInteractableInterface, public IPickupableInterface, public IHoldableInterface
{
	GENERATED_BODY()

public:
	ABaseWeapon();

protected:
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ============================================
	// COMPONENTS
	// ============================================

	// First-person mesh (visible only to owner)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	USkeletalMeshComponent* FPSMesh;

	// Third-person mesh (visible to everyone except owner)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	USkeletalMeshComponent* TPSMesh;

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

	// Fire rate in rounds per minute (RPM)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float FireRate = 600.0f;

	// Weapon spread/accuracy (0 = perfect accuracy, higher = more spread)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float Spread = 0.5f;

	// Recoil intensity multiplier
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Shooting")
	float RecoilScale = 1.0f;

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

	// Socket name on CHARACTER where weapon meshes attach
	// FPS mesh attaches to Arms mesh at this socket
	// TPS mesh attaches to Body mesh at this socket
	// Common values: "weapon_r", "hand_r", "weapon_socket"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName CharacterAttachSocket = FName("weapon_r");

	// ============================================
	// PICKUP / DROP
	// ============================================

	// Throw impulse strength when dropping weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Pickup")
	float DropImpulseStrength = 500.0f;

	// ============================================
	// STATE
	// ============================================

	// Is weapon currently reloading (runtime state, NOT replicated)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|State")
	bool IsReload = false;

	// ============================================
	// AMMO SYSTEM
	// ============================================

	// Accepted caliber type (for ammo compatibility checks)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Ammo")
	EAmmoCaliberType AcceptedCaliberType = EAmmoCaliberType::NATO_556x45mm;

	// ============================================
	// MAGAZINE SYSTEM
	// ============================================

	// Current attached magazine reference (REPLICATED)
	UPROPERTY(BlueprintReadWrite, Category = "Weapon|Magazine", Replicated)
	ABaseMagazine* CurrentMagazine = nullptr;

	// Magazine class for spawning/initialization
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Magazine")
	TSubclassOf<ABaseMagazine> MagazineClass;

	/**
	 * Check if this weapon can accept specific ammo type
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool CanAcceptAmmoType(EAmmoCaliberType CaliberType) const
	{
		return AcceptedCaliberType == CaliberType;
	}

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

	// Get FPS mesh component (generic interface)
	virtual UMeshComponent* GetFPSMeshComponent_Implementation() const override;

	// Get TPS mesh component (generic interface)
	virtual UMeshComponent* GetTPSMeshComponent_Implementation() const override;

	// Get attachment socket name (same for FPS and TPS)
	virtual FName GetAttachSocket_Implementation() const override;

	// Get animation layer class
	virtual TSubclassOf<UAnimInstance> GetAnimLayer_Implementation() const override;

	// ============================================
	// DEPRECATED - Use IHoldableInterface methods instead
	// ============================================

	// Get FPS mesh component (visible only to owner)
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh", meta = (DeprecatedFunction, DeprecationMessage = "Use IHoldableInterface::GetFPSMeshComponent instead"))
	USkeletalMeshComponent* GetFPSMesh() const { return FPSMesh; }

	// Get TPS mesh component (visible to everyone except owner)
	UFUNCTION(BlueprintPure, Category = "Weapon|Mesh", meta = (DeprecatedFunction, DeprecationMessage = "Use IHoldableInterface::GetTPSMeshComponent instead"))
	USkeletalMeshComponent* GetTPSMesh() const { return TPSMesh; }

	// Get aim hands offset (when aiming down sights)
	UFUNCTION(BlueprintPure, Category = "Weapon|Hands")
	FVector GetAimHandsOffset() const { return AimHandsOffset; }
};
