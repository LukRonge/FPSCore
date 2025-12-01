// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/BaseGrenade.h"
#include "Interfaces/ViewPointProviderInterface.h"
#include "Interfaces/ProjectileInterface.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Core/FPSGameplayTags.h"
#include "Net/UnrealNetwork.h"

ABaseGrenade::ABaseGrenade()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replication setup
	bReplicates = true;
	bAlwaysRelevant = false;

	// Create scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Create FPS mesh (visible only to owner)
	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetOwnerNoSee(false);
	FPSMesh->SetCastShadow(false);
	FPSMesh->bReceivesDecals = false;
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create TPS mesh (visible to other players)
	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(SceneRoot);
	TPSMesh->SetOnlyOwnerSee(false);
	TPSMesh->SetOwnerNoSee(true);
	TPSMesh->SetCastShadow(true);
	TPSMesh->bReceivesDecals = true;
	TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TPSMesh->SetSimulatePhysics(true);
	TPSMesh->SetIsReplicated(true);
	TPSMesh->bReplicatePhysicsToAutonomousProxy = true;
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
	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::UseStart - bHasThrown=%d, bIsThrowing=%d"), bHasThrown, bIsThrowing);

	// Validate state
	if (bHasThrown || bIsThrowing)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::UseStart - Already thrown or throwing, aborting"));
		return;
	}

	// Mark as throwing
	bIsThrowing = true;

	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::UseStart - Calling Multicast_PlayThrowEffects"));

	// Play throw animation on all clients
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
	// Standard pickup - will be handled by inventory system
}

void ABaseGrenade::OnDropped_Implementation(const FInteractionContext& Ctx)
{
	// Reset visibility for world pickup
	ResetVisibilityForWorld();

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
	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::OnThrowRelease - Called, OwningPawn=%s, bHasThrown=%d"),
		OwningPawn ? *OwningPawn->GetName() : TEXT("NULL"), bHasThrown);

	// Called from AnimNotify_ThrowRelease via IThrowableInterface
	if (!OwningPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::OnThrowRelease - No owning pawn"));
		return;
	}

	if (bHasThrown)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::OnThrowRelease - Already thrown"));
		return;
	}

	// Get spawn location from FPS mesh socket
	FVector SpawnLocation = FVector::ZeroVector;
	if (FPSMesh && FPSMesh->DoesSocketExist(ProjectileSpawnSocket))
	{
		SpawnLocation = FPSMesh->GetSocketLocation(ProjectileSpawnSocket);
		UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::OnThrowRelease - SpawnLocation from socket %s: %s"),
			*ProjectileSpawnSocket.ToString(), *SpawnLocation.ToString());
	}
	else
	{
		// Fallback to actor location
		SpawnLocation = GetActorLocation();
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::OnThrowRelease - Socket %s not found, using actor location: %s"),
			*ProjectileSpawnSocket.ToString(), *SpawnLocation.ToString());
	}

	// Get throw direction from camera via IViewPointProviderInterface
	FVector ThrowDirection = FVector::ForwardVector;
	if (OwningPawn->Implements<UViewPointProviderInterface>())
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		IViewPointProviderInterface::Execute_GetShootingViewPoint(OwningPawn, CameraLocation, CameraRotation);
		ThrowDirection = CameraRotation.Vector();
		UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::OnThrowRelease - ThrowDirection from camera: %s"), *ThrowDirection.ToString());
	}
	else
	{
		// Fallback to pawn forward
		ThrowDirection = OwningPawn->GetActorForwardVector();
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::OnThrowRelease - Pawn doesn't implement ViewPointProviderInterface, using forward: %s"),
			*ThrowDirection.ToString());
	}

	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::OnThrowRelease - Calling Server_ExecuteThrow"));

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
	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Server_ExecuteThrow - SpawnLocation=%s, ThrowDirection=%s, HasAuthority=%d"),
		*SpawnLocation.ToString(), *ThrowDirection.ToString(), HasAuthority());

	// SERVER ONLY
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::Server_ExecuteThrow - Not authority, aborting"));
		return;
	}

	// Validate state
	if (bHasThrown)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::Server_ExecuteThrow - Already thrown"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Server_ExecuteThrow - Calling SpawnProjectile, ProjectileClass=%s"),
		ProjectileClass ? *ProjectileClass->GetName() : TEXT("NULL"));

	// Spawn projectile via interface
	AActor* Projectile = SpawnProjectile(SpawnLocation, ThrowDirection);

	if (Projectile)
	{
		// Set replicated state - triggers OnRep on clients
		bHasThrown = true;

		UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Server_ExecuteThrow - Projectile spawned: %s, destroying grenade"), *Projectile->GetName());

		// Destroy grenade actor after successful throw (server handles replication)
		Destroy();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseGrenade::Server_ExecuteThrow - Failed to spawn projectile"));
		bIsThrowing = false;
	}
}

AActor* ABaseGrenade::SpawnProjectile(FVector SpawnLocation, FVector ThrowDirection)
{
	// SERVER ONLY
	if (!HasAuthority())
	{
		return nullptr;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseGrenade::SpawnProjectile - No ProjectileClass set"));
		return nullptr;
	}

	// Verify projectile class implements IProjectileInterface
	if (!ProjectileClass->ImplementsInterface(UProjectileInterface::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseGrenade::SpawnProjectile - ProjectileClass does not implement IProjectileInterface"));
		return nullptr;
	}

	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = OwningPawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Spawn projectile actor
	AActor* Projectile = GetWorld()->SpawnActor<AActor>(
		ProjectileClass,
		SpawnLocation,
		ThrowDirection.Rotation(),
		SpawnParams
	);

	if (Projectile)
	{
		// Initialize projectile via IProjectileInterface (NO DIRECT CLASS REFERENCE)
		// Projectile uses its rotation and ProjectileMovementComponent::InitialSpeed
		IProjectileInterface::Execute_InitializeProjectile(Projectile, OwningPawn);
	}

	return Projectile;
}

void ABaseGrenade::Multicast_PlayThrowEffects_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - OwningPawn=%s, ThrowMontage=%s"),
		OwningPawn ? *OwningPawn->GetName() : TEXT("NULL"),
		ThrowMontage ? *ThrowMontage->GetName() : TEXT("NULL"));

	bIsThrowing = true;

	// Play throw montage on character meshes via ICharacterMeshProviderInterface
	if (!OwningPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - No OwningPawn"));
		return;
	}

	if (!ThrowMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - No ThrowMontage set"));
		return;
	}

	if (!OwningPawn->Implements<UCharacterMeshProviderInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - Pawn doesn't implement CharacterMeshProviderInterface"));
		return;
	}

	// Get character meshes
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(OwningPawn);
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(OwningPawn);
	USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(OwningPawn);

	// Play montage on Arms
	if (ArmsMesh && ArmsMesh->GetAnimInstance())
	{
		ArmsMesh->GetAnimInstance()->Montage_Play(ThrowMontage);
		UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - Playing on Arms"));
	}

	// Play montage on Body
	if (BodyMesh && BodyMesh->GetAnimInstance())
	{
		BodyMesh->GetAnimInstance()->Montage_Play(ThrowMontage);
		UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - Playing on Body"));
	}

	// Play montage on Legs
	if (LegsMesh && LegsMesh->GetAnimInstance())
	{
		LegsMesh->GetAnimInstance()->Montage_Play(ThrowMontage);
		UE_LOG(LogTemp, Log, TEXT("ABaseGrenade::Multicast_PlayThrowEffects - Playing on Legs"));
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

	// FPS mesh - only owner sees
	if (FPSMesh)
	{
		FPSMesh->SetOnlyOwnerSee(true);
		FPSMesh->SetOwnerNoSee(false);
		FPSMesh->SetCastShadow(false);
	}

	// TPS mesh - everyone except owner sees
	if (TPSMesh)
	{
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
