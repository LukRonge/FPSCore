// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/BaseGrenade.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/ProjectileInterface.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Core/FPSGameplayTags.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPSCore, Log, All);

ABaseGrenade::ABaseGrenade()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replication setup
	bReplicates = true;
	bAlwaysRelevant = false;

	// Network optimization
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(2.0f);

	// Create scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Create FPS mesh (visible only to owner - first person view)
	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);
	FPSMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetOwnerNoSee(false);
	FPSMesh->SetCastShadow(false);
	FPSMesh->bReceivesDecals = false;
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FPSMesh->SetSimulatePhysics(false);
	// Performance optimizations
	FPSMesh->bComponentUseFixedSkelBounds = true;
	FPSMesh->SetGenerateOverlapEvents(false);

	// Create TPS mesh (visible to other players - third person/world view)
	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(SceneRoot);
	TPSMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	TPSMesh->SetOnlyOwnerSee(false);
	TPSMesh->SetOwnerNoSee(true);
	TPSMesh->SetCastShadow(true);
	TPSMesh->bReceivesDecals = true;
	TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TPSMesh->SetSimulatePhysics(true);
	TPSMesh->SetIsReplicated(true);
	TPSMesh->bReplicatePhysicsToAutonomousProxy = true;
	// Collision setup - ignore characters, allow world collision for physics
	TPSMesh->SetCollisionObjectType(ECC_PhysicsBody);
	TPSMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	TPSMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	TPSMesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	TPSMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Ignore);
	TPSMesh->CanCharacterStepUpOn = ECB_No;
	TPSMesh->SetGenerateOverlapEvents(false);
	// Performance optimizations
	TPSMesh->bEnableUpdateRateOptimizations = true;
	TPSMesh->bComponentUseFixedSkelBounds = true;
}

void ABaseGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseGrenade, bHasThrown);
}

void ABaseGrenade::BeginPlay()
{
	Super::BeginPlay();
}

void ABaseGrenade::OnRep_Owner()
{
	Super::OnRep_Owner();

	// Setup visibility when owner replicates
	if (GetOwner() && !bVisibilityInitialized)
	{
		SetupMeshVisibility();
	}
}

void ABaseGrenade::OnRep_HasThrown()
{
	// Client callback when bHasThrown replicates
	// Hide meshes since grenade has been thrown
	if (bHasThrown)
	{
		if (FPSMesh)
		{
			FPSMesh->SetVisibility(false);
		}
		if (TPSMesh)
		{
			TPSMesh->SetVisibility(false);
		}
	}
}

// ============================================
// IHoldableInterface Implementation
// ============================================

void ABaseGrenade::OnEquipped_Implementation(APawn* NewOwner)
{
	if (!NewOwner) return;

	OwningPawn = NewOwner;
	bIsEquipped = true;

	// Set owner for replication
	SetOwner(NewOwner);

	// Setup visibility
	SetupMeshVisibility();
}

void ABaseGrenade::OnUnequipped_Implementation()
{
	bIsEquipped = false;
	bIsEquipping = false;
	bIsUnequipping = false;
	bIsThrowing = false;
}

void ABaseGrenade::SetFPSMeshVisibility_Implementation(bool bVisible)
{
	if (FPSMesh)
	{
		FPSMesh->SetVisibility(bVisible, true);
	}
}

bool ABaseGrenade::CanBeUnequipped_Implementation() const
{
	// Cannot unequip during throw or after thrown
	return !bIsThrowing && !bHasThrown;
}

void ABaseGrenade::OnEquipMontageComplete_Implementation(APawn* NewOwner)
{
	bIsEquipping = false;
}

void ABaseGrenade::OnUnequipMontageComplete_Implementation(APawn* NewOwner)
{
	bIsUnequipping = false;
}

// ============================================
// IUsableInterface Implementation
// ============================================

bool ABaseGrenade::CanUse_Implementation(const FUseContext& Ctx) const
{
	// Cannot use if already thrown or throwing
	if (bHasThrown || bIsThrowing)
	{
		return false;
	}

	// Cannot use during equip/unequip
	if (bIsEquipping || bIsUnequipping)
	{
		return false;
	}

	return true;
}

void ABaseGrenade::UseStart_Implementation(const FUseContext& Ctx)
{
	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] UseStart - %s - bHasThrown=%d, bIsThrowing=%d, bIsEquipping=%d, bIsUnequipping=%d, Role=%s"),
		*GetName(), bHasThrown, bIsThrowing, bIsEquipping, bIsUnequipping,
		*UEnum::GetValueAsString(GetLocalRole()));

	// LOCAL CHECK - prevents multiple Server RPCs for same throw
	// This is optimistic local prediction - server will validate
	if (bIsThrowing || bHasThrown)
	{
		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] UseStart - Already throwing or thrown, skipping RPC"));
		return;
	}

	// Set local flag immediately (optimistic prediction)
	// Server will authorize the actual throw via Server_StartThrow
	bIsThrowing = true;

	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] UseStart - Calling Server_StartThrow"));

	// Client calls Server_StartThrow - server will validate and trigger multicast
	// This follows the same pattern as BaseWeapon::UseStart → Server_Shoot
	Server_StartThrow();
}

void ABaseGrenade::Server_StartThrow_Implementation()
{
	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_StartThrow - %s - bHasThrown=%d, bIsThrowing=%d, bIsEquipping=%d, bIsUnequipping=%d, HasAuthority=%d"),
		*GetName(), bHasThrown, bIsThrowing, bIsEquipping, bIsUnequipping, HasAuthority());

	// SERVER ONLY - validate state
	// NOTE: Don't check bIsThrowing here! On Listen Server, UseStart sets bIsThrowing=true
	// BEFORE this RPC executes (same frame, direct call). Only check bHasThrown (replicated).
	if (bHasThrown)
	{
		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_StartThrow - Already thrown, aborting"));
		return;
	}

	if (bIsEquipping || bIsUnequipping)
	{
		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_StartThrow - Equip/Unequip in progress, aborting"));
		return;
	}

	// Mark as throwing on server (for remote clients via Multicast)
	bIsThrowing = true;

	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_StartThrow - Calling Multicast_PlayThrowEffects, ThrowMontage=%s"),
		ThrowMontage ? *ThrowMontage->GetName() : TEXT("NULL"));

	// SERVER triggers multicast - this is the correct pattern
	// Multicast called from server reaches ALL clients including the owning client
	Multicast_PlayThrowEffects();
}

// ============================================
// IPickupableInterface Implementation
// ============================================

bool ABaseGrenade::CanBePicked_Implementation(const FInteractionContext& Ctx) const
{
	// Cannot pick up if already thrown
	return !bHasThrown;
}

void ABaseGrenade::OnPicked_Implementation(APawn* Picker, const FInteractionContext& Ctx)
{
	// Disable TPS mesh physics when picked up (will be attached to character)
	if (TPSMesh)
	{
		TPSMesh->SetSimulatePhysics(false);
		TPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABaseGrenade::OnDropped_Implementation(const FInteractionContext& Ctx)
{
	// Reset visibility for world pickup
	ResetVisibilityForWorld();

	// Re-enable TPS mesh physics for world pickup
	if (TPSMesh)
	{
		TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TPSMesh->SetSimulatePhysics(true);
	}

	// Clear owner
	SetOwner(nullptr);
	OwningPawn = nullptr;

	// Reset state
	bIsEquipped = false;
	bIsEquipping = false;
	bIsUnequipping = false;
}

// ============================================
// IThrowableInterface Implementation
// ============================================

void ABaseGrenade::OnThrowRelease_Implementation()
{
	// Use GetOwner() for consistency - it's replicated via SetOwner()
	// OwningPawn is set in OnEquipped but GetOwner() is the authoritative source
	APawn* CurrentOwner = Cast<APawn>(GetOwner());

	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] OnThrowRelease - %s - Owner=%s, OwningPawn=%s, bHasThrown=%d, bIsThrowing=%d, Role=%s, HasAuthority=%d"),
		*GetName(),
		CurrentOwner ? *CurrentOwner->GetName() : TEXT("NULL"),
		OwningPawn ? *OwningPawn->GetName() : TEXT("NULL"),
		bHasThrown, bIsThrowing,
		*UEnum::GetValueAsString(GetLocalRole()),
		HasAuthority());

	// Called from AnimNotify_ThrowRelease via IThrowableInterface
	// Use CurrentOwner (from GetOwner()) as it's replicated and more reliable
	if (!CurrentOwner)
	{
		UE_LOG(LogFPSCore, Warning, TEXT("ABaseGrenade::OnThrowRelease - No owner (GetOwner returned nullptr)"));
		return;
	}

	// Sync OwningPawn if needed (defensive)
	if (!OwningPawn)
	{
		OwningPawn = CurrentOwner;
	}

	if (bHasThrown)
	{
		UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::OnThrowRelease - Already thrown"));
		return;
	}

	// Get spawn location from character's Arms mesh socket (grenade is attached there)
	FVector SpawnLocation = FVector::ZeroVector;

	// Try to get spawn location from character's Arms mesh
	if (CurrentOwner->Implements<UCharacterMeshProviderInterface>())
	{
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CurrentOwner);
		if (ArmsMesh && ArmsMesh->DoesSocketExist(CharacterAttachSocket))
		{
			SpawnLocation = ArmsMesh->GetSocketLocation(CharacterAttachSocket);
			UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::OnThrowRelease - SpawnLocation from Arms socket %s: %s"),
				*CharacterAttachSocket.ToString(), *SpawnLocation.ToString());
		}
		else
		{
			// Fallback to actor location
			SpawnLocation = GetActorLocation();
			UE_LOG(LogFPSCore, Warning, TEXT("ABaseGrenade::OnThrowRelease - Socket %s not found on Arms mesh, using actor location: %s"),
				*CharacterAttachSocket.ToString(), *SpawnLocation.ToString());
		}
	}
	else
	{
		// Fallback to actor location
		SpawnLocation = GetActorLocation();
		UE_LOG(LogFPSCore, Warning, TEXT("ABaseGrenade::OnThrowRelease - Pawn doesn't implement CharacterMeshProviderInterface, using actor location: %s"),
			*SpawnLocation.ToString());
	}

	// Get throw direction from camera via IViewPointProviderInterface
	FVector ThrowDirection = FVector::ForwardVector;
	if (CurrentOwner->Implements<UViewPointProviderInterface>())
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		IViewPointProviderInterface::Execute_GetShootingViewPoint(CurrentOwner, CameraLocation, CameraRotation);
		ThrowDirection = CameraRotation.Vector();
		UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::OnThrowRelease - ThrowDirection from camera: %s"), *ThrowDirection.ToString());
	}
	else
	{
		// Fallback to pawn forward
		ThrowDirection = CurrentOwner->GetActorForwardVector();
		UE_LOG(LogFPSCore, Warning, TEXT("ABaseGrenade::OnThrowRelease - Pawn doesn't implement ViewPointProviderInterface, using forward: %s"),
			*ThrowDirection.ToString());
	}

	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] OnThrowRelease - Calling Server_ExecuteThrow, SpawnLocation=%s, ThrowDirection=%s"),
		*SpawnLocation.ToString(), *ThrowDirection.ToString());

	// Request server to execute throw
	Server_ExecuteThrow(SpawnLocation, ThrowDirection);
}

bool ABaseGrenade::CanThrow_Implementation() const
{
	// Cannot throw if already thrown or throwing
	if (bHasThrown || bIsThrowing)
	{
		return false;
	}

	// Cannot throw during equip/unequip
	if (bIsEquipping || bIsUnequipping)
	{
		return false;
	}

	return true;
}

void ABaseGrenade::Server_ExecuteThrow_Implementation(FVector SpawnLocation, FVector ThrowDirection)
{
	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - %s - SpawnLocation=%s, ThrowDirection=%s, HasAuthority=%d, bHasThrown=%d"),
		*GetName(), *SpawnLocation.ToString(), *ThrowDirection.ToString(), HasAuthority(), bHasThrown);

	// SERVER ONLY
	if (!HasAuthority())
	{
		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Not authority, aborting"));
		return;
	}

	// Validate state
	if (bHasThrown)
	{
		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Already thrown, aborting"));
		return;
	}

	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Calling SpawnProjectile, ProjectileClass=%s"),
		ProjectileClass ? *ProjectileClass->GetName() : TEXT("NULL"));

	// Spawn projectile via interface
	AActor* Projectile = SpawnProjectile(SpawnLocation, ThrowDirection);

	if (Projectile)
	{
		// Set replicated state - triggers OnRep on clients
		bHasThrown = true;

		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Projectile spawned: %s"), *Projectile->GetName());

		// Remove grenade from character's inventory BEFORE destroying
		// This triggers OnInventoryItemRemoved which:
		// - Clears ActiveItem on character
		// - Updates anim layer to default
		// - Handles all cleanup properly
		if (OwningPawn && OwningPawn->Implements<UItemCollectorInterface>())
		{
			UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Removing grenade from inventory via Drop"));
			// Using Drop will call RemoveItem internally and handle all cleanup
			// The grenade will be detached but we destroy it immediately after
			IItemCollectorInterface::Execute_Drop(OwningPawn, this);
		}
		else
		{
			UE_LOG(LogFPSCore, Warning, TEXT("[GRENADE_THROW] Server_ExecuteThrow - OwningPawn is NULL or doesn't implement ItemCollectorInterface!"));
		}

		UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Destroying grenade actor"));

		// Destroy grenade actor after successful throw (server handles replication)
		Destroy();
	}
	else
	{
		UE_LOG(LogFPSCore, Error, TEXT("[GRENADE_THROW] Server_ExecuteThrow - Failed to spawn projectile! Check ProjectileClass and IProjectileInterface."));
		bIsThrowing = false;
	}
}

AActor* ABaseGrenade::SpawnProjectile(FVector SpawnLocation, FVector ThrowDirection)
{
	UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::SpawnProjectile - %s - Location=%s, Direction=%s"),
		*GetName(), *SpawnLocation.ToString(), *ThrowDirection.ToString());

	// SERVER ONLY
	if (!HasAuthority())
	{
		UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::SpawnProjectile - Not authority"));
		return nullptr;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogFPSCore, Error, TEXT("ABaseGrenade::SpawnProjectile - No ProjectileClass set! Configure ProjectileClass in Blueprint defaults."));
		return nullptr;
	}

	// Verify projectile class implements IProjectileInterface
	if (!ProjectileClass->ImplementsInterface(UProjectileInterface::StaticClass()))
	{
		UE_LOG(LogFPSCore, Error, TEXT("ABaseGrenade::SpawnProjectile - ProjectileClass %s does not implement IProjectileInterface!"),
			*ProjectileClass->GetName());
		return nullptr;
	}

	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	// Owner = OwningPawn (the character), NOT 'this' (the grenade which will be destroyed)
	// This prevents issues when the projectile tries to access its owner after the grenade is gone
	SpawnParams.Owner = OwningPawn;
	SpawnParams.Instigator = OwningPawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::SpawnProjectile - Spawning %s at %s with rotation %s"),
		*ProjectileClass->GetName(), *SpawnLocation.ToString(), *ThrowDirection.Rotation().ToString());

	// Spawn projectile actor
	AActor* Projectile = GetWorld()->SpawnActor<AActor>(
		ProjectileClass,
		SpawnLocation,
		ThrowDirection.Rotation(),
		SpawnParams
	);

	if (Projectile)
	{
		UE_LOG(LogFPSCore, Verbose, TEXT("ABaseGrenade::SpawnProjectile - Spawned %s, calling InitializeProjectile"), *Projectile->GetName());
		// Initialize projectile via IProjectileInterface (NO DIRECT CLASS REFERENCE)
		// Projectile uses its rotation and ProjectileMovementComponent::InitialSpeed
		IProjectileInterface::Execute_InitializeProjectile(Projectile, OwningPawn);
	}
	else
	{
		UE_LOG(LogFPSCore, Error, TEXT("ABaseGrenade::SpawnProjectile - SpawnActor returned nullptr!"));
	}

	return Projectile;
}

void ABaseGrenade::Multicast_PlayThrowEffects_Implementation()
{
	// ============================================
	// STEP 1: DETERMINE VIEW PERSPECTIVE
	// ============================================
	// Use GetOwner() - it's replicated via SetOwner()
	// OwningPawn is local-only, not available on remote clients
	AActor* GrenadeOwner = GetOwner();
	APawn* OwnerPawn = GrenadeOwner ? Cast<APawn>(GrenadeOwner) : nullptr;
	const bool bIsLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();

	UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - %s - Owner=%s, bIsLocallyControlled=%d, ThrowMontage=%s, Role=%s"),
		*GetName(),
		GrenadeOwner ? *GrenadeOwner->GetName() : TEXT("NULL"),
		bIsLocallyControlled,
		ThrowMontage ? *ThrowMontage->GetName() : TEXT("NULL"),
		*UEnum::GetValueAsString(GetLocalRole()));

	// NOTE: bIsThrowing is set in UseStart (local) and Server_StartThrow (server)
	// Remote clients receive this via multicast but don't need local state tracking
	// since they just play the animation and the grenade will be destroyed after throw

	// ============================================
	// STEP 2: VALIDATION
	// ============================================
	if (!ThrowMontage || !GrenadeOwner)
	{
		UE_LOG(LogFPSCore, Warning, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - Missing ThrowMontage=%s or Owner=%s"),
			ThrowMontage ? TEXT("OK") : TEXT("NULL"),
			GrenadeOwner ? TEXT("OK") : TEXT("NULL"));
		return;
	}

	if (!GrenadeOwner->Implements<UCharacterMeshProviderInterface>())
	{
		UE_LOG(LogFPSCore, Warning, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - Owner doesn't implement CharacterMeshProviderInterface"));
		return;
	}

	// ============================================
	// STEP 3: THROW ANIMATIONS
	// ============================================
	// Animations run on ALL machines (including dedicated server)
	// Visibility is handled by mesh settings (OwnerNoSee, OnlyOwnerSee)

	// --- FPS ANIMATIONS (Owning client only) ---
	// Arms mesh has OnlyOwnerSee, so only owning client sees this
	if (bIsLocallyControlled)
	{
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(GrenadeOwner);
		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(ThrowMontage);
				UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - Playing ThrowMontage on Arms (FPS)"));
			}
			else
			{
				UE_LOG(LogFPSCore, Warning, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - No AnimInstance on Arms mesh"));
			}
		}
		else
		{
			UE_LOG(LogFPSCore, Warning, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - No Arms mesh"));
		}
	}

	// --- TPS ANIMATIONS (All machines for remote view) ---
	// Body/Legs have OwnerNoSee, so owning client won't see them rendering
	// But animation still plays for physics synchronization and remote clients
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(GrenadeOwner);
	if (BodyMesh)
	{
		if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(ThrowMontage);
			UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - Playing ThrowMontage on Body (TPS)"));
		}
	}

	USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(GrenadeOwner);
	if (LegsMesh)
	{
		if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(ThrowMontage);
			UE_LOG(LogFPSCore, Log, TEXT("[GRENADE_THROW] Multicast_PlayThrowEffects - Playing ThrowMontage on Legs (TPS)"));
		}
	}
}

// ============================================
// Visibility
// ============================================

void ABaseGrenade::SetupMeshVisibility()
{
	if (bVisibilityInitialized)
	{
		return;
	}

	AActor* CurrentOwner = GetOwner();
	if (!CurrentOwner)
	{
		return;
	}

	// FPS mesh - only owner sees (first person view)
	if (FPSMesh)
	{
		FPSMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		FPSMesh->SetOnlyOwnerSee(true);
		FPSMesh->SetOwnerNoSee(false);
		FPSMesh->SetCastShadow(false);
	}

	// TPS mesh - everyone except owner sees (world space)
	if (TPSMesh)
	{
		TPSMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		TPSMesh->SetOnlyOwnerSee(false);
		TPSMesh->SetOwnerNoSee(true);
		TPSMesh->SetCastShadow(true);
	}

	bVisibilityInitialized = true;
}

void ABaseGrenade::ResetVisibilityForWorld()
{
	// Reset for world pickup - only TPS mesh visible to everyone
	if (FPSMesh)
	{
		FPSMesh->SetVisibility(false);
		FPSMesh->SetOnlyOwnerSee(false);
	}

	if (TPSMesh)
	{
		TPSMesh->SetVisibility(true);
		TPSMesh->SetOwnerNoSee(false);
		TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Reset flag so visibility will be reapplied on next pickup
	bVisibilityInitialized = false;
}

// ============================================
// IInteractableInterface Implementation
// ============================================

void ABaseGrenade::GetVerbs_Implementation(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const
{
	OutVerbs.Add(FPSGameplayTags::Interact_Pickup);
}

bool ABaseGrenade::CanInteract_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const
{
	if (Verb == FPSGameplayTags::Interact_Pickup)
	{
		return GetOwner() == nullptr && !bHasThrown;
	}
	return false;
}

void ABaseGrenade::Interact_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx)
{
	if (Verb == FPSGameplayTags::Interact_Pickup && IPickupableInterface::Execute_CanBePicked(this, Ctx))
	{
		if (GetOwner() == nullptr && Ctx.Pawn && Ctx.Pawn->Implements<UItemCollectorInterface>())
		{
			IItemCollectorInterface::Execute_Pickup(Ctx.Pawn, this);
		}
	}
}

FText ABaseGrenade::GetInteractionText_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const
{
	if (Verb == FPSGameplayTags::Interact_Pickup)
	{
		return FText::Format(FText::FromString("Pickup {0}"), Name);
	}
	return FText::GetEmpty();
}
