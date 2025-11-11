// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeapon.h"
#include "Components/BallisticsComponent.h"
#include "Components/FireComponent.h"
#include "Net/UnrealNetwork.h"
#include "Core/FPSGameplayTags.h"
#include "FPSCharacter.h"
#include "BaseMagazine.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// ============================================
	// DUAL-MESH SETUP (FPS + TPS SIBLINGS)
	// ============================================

	// Scene Root (empty component, hierarchy root)
	// Prevents transform inheritance between FPS and TPS meshes
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// TPS Mesh (sibling of FPS, child of SceneRoot)
	// Visible to other players, attached to character Body
	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(SceneRoot);                   // Sibling relationship
	TPSMesh->SetOwnerNoSee(true);                          // Hide from owner (they see FPS mesh)
	TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TPSMesh->SetIsReplicated(true);                        // Replicate component transform/physics

	// Physics replication settings (critical for dropped weapons)
	TPSMesh->bReplicatePhysicsToAutonomousProxy = true;    // Replicate to owning client

	// FPS Mesh (sibling of TPS, child of SceneRoot)
	// Visible only to owner, attached to character Arms
	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);                   // Sibling relationship (NOT child of TPSMesh!)
	FPSMesh->SetOnlyOwnerSee(true);                        // Only owner sees this                 
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // No collision (optimization)

	// ============================================
	// BALLISTICS COMPONENT
	// ============================================

	// Create ballistics component (handles ammo, magazine, ballistic calculations)
	BallisticsComponent = CreateDefaultSubobject<UBallisticsComponent>(TEXT("BallisticsComponent"));

	// ============================================
	// MAGAZINE COMPONENTS (FPS + TPS)
	// ============================================

	// FPS Magazine (attached to FPS mesh, visible only to owner)
	FPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("FPSMagazineComponent"));
	FPSMagazineComponent->SetupAttachment(FPSMesh, FName("magazine"));
	// Child Actor Class will be set from MagazineClass in BeginPlay

	// TPS Magazine (attached to TPS mesh, visible to others)
	TPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("TPSMagazineComponent"));
	TPSMagazineComponent->SetupAttachment(TPSMesh, FName("magazine"));
	// Child Actor Class will be set from MagazineClass in PostInitializeComponents
}

void ABaseWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// ============================================
	// SYNC FPS MESH WITH TPS MESH
	// ============================================

	if (FPSMesh && TPSMesh && TPSMesh->GetSkeletalMeshAsset())
	{
		// Copy mesh asset from TPS to FPS
		FPSMesh->SetSkeletalMeshAsset(TPSMesh->GetSkeletalMeshAsset());

		// Copy animation blueprint (if any)
		if (TPSMesh->GetAnimClass())
		{
			FPSMesh->SetAnimInstanceClass(TPSMesh->GetAnimClass());
		}

		UE_LOG(LogTemp, Log, TEXT("✓ BaseWeapon::PostInitializeComponents() - FPSMesh synced with TPSMesh: %s"),
			*TPSMesh->GetSkeletalMeshAsset()->GetName());
	}

	// ============================================
	// SPAWN MAGAZINE ACTORS (via SetChildActorClass)
	// ============================================
	// MagazineClass is now available (Blueprint properties loaded)
	// This spawns the child actors automatically

	if (MagazineClass)
	{
		// Spawn FPS magazine actor
		if (FPSMagazineComponent)
		{
			FPSMagazineComponent->SetChildActorClass(MagazineClass);
			UE_LOG(LogTemp, Log, TEXT("✓ BaseWeapon::PostInitializeComponents() - FPSMagazineComponent spawned: %s"),
				*MagazineClass->GetName());
		}

		// Spawn TPS magazine actor
		if (TPSMagazineComponent)
		{
			TPSMagazineComponent->SetChildActorClass(MagazineClass);
			UE_LOG(LogTemp, Log, TEXT("✓ BaseWeapon::PostInitializeComponents() - TPSMagazineComponent spawned: %s"),
				*MagazineClass->GetName());
		}
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	// ============================================
	// ASSIGN CurrentMagazine (AUTHORITATIVE SOURCE)
	// ============================================
	// CurrentMagazine points to TPS magazine actor (gameplay logic uses this)
	// TPS magazine is authoritative, FPS magazine is visual copy only
	// NOTE: Owner is NULL at this point - will be set later in SetOwner() when equipped

	if (TPSMagazineComponent && TPSMagazineComponent->GetChildActor())
	{
		CurrentMagazine = Cast<ABaseMagazine>(TPSMagazineComponent->GetChildActor());

		if (CurrentMagazine)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ BaseWeapon::BeginPlay() - CurrentMagazine = TPS Magazine: %s"),
				*CurrentMagazine->GetName());
		}
	}

	// ============================================
	// COMPONENT SYNCHRONIZATION
	// ============================================
	// Synchronize FireComponent with BallisticsComponent and CurrentMagazine
	// FireComponent properties (FireRate, Spread, RecoilScale) are configured in Blueprint defaults

	if (FireComponent && BallisticsComponent && CurrentMagazine)
	{
		// Set references in FireComponent
		FireComponent->CurrentMagazine = CurrentMagazine;
		FireComponent->BallisticsComponent = BallisticsComponent;

		UE_LOG(LogTemp, Log, TEXT("✓ BaseWeapon::BeginPlay() - FireComponent synchronized (Magazine: %s)"),
			*CurrentMagazine->GetName());
	}
	else
	{
		if (!FireComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠ BaseWeapon::BeginPlay() - FireComponent is NULL! Set in Blueprint."));
		}
		if (!BallisticsComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠ BaseWeapon::BeginPlay() - BallisticsComponent is NULL!"));
		}
		if (!CurrentMagazine)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠ BaseWeapon::BeginPlay() - CurrentMagazine is NULL!"));
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Only magazine reference is replicated
	DOREPLIFETIME(ABaseWeapon, CurrentMagazine);
}

void ABaseWeapon::OnRep_CurrentMagazine()
{
	// Called on CLIENTS when CurrentMagazine is replicated from server
	// Synchronize magazine reference to FireComponent

	if (FireComponent)
	{
		FireComponent->CurrentMagazine = CurrentMagazine;

		FString MagazineName = CurrentMagazine ? CurrentMagazine->GetName() : TEXT("nullptr");
		UE_LOG(LogTemp, Log, TEXT("BaseWeapon::OnRep_CurrentMagazine() - Synced to FireComponent: %s"), *MagazineName);
	}

	// TODO: Trigger UI update events
	// TODO: Play magazine change animations
	// TODO: Update HUD ammo display
}

void ABaseWeapon::SetOwner(AActor* NewOwner)
{
	// Call parent implementation
	Super::SetOwner(NewOwner);

	// Propagate owner to magazines (runs on Server + Clients via replication)
	// Owner chain: Character → Weapon → Magazines
	// Rendering flags (SetOnlyOwnerSee/SetOwnerNoSee) are already set in Blueprint defaults
	// based on FirstPersonPrimitiveType - they handle visibility automatically

	FString MachineRole = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

	// Propagate to FPS magazine
	if (FPSMagazineComponent && FPSMagazineComponent->GetChildActor())
	{
		FPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
		UE_LOG(LogTemp, Log, TEXT("[%s] FPS Magazine owner set: %s"),
			*MachineRole, NewOwner ? *NewOwner->GetName() : TEXT("nullptr"));
	}

	// Propagate to TPS magazine
	if (TPSMagazineComponent && TPSMagazineComponent->GetChildActor())
	{
		TPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
		UE_LOG(LogTemp, Log, TEXT("[%s] TPS Magazine owner set: %s"),
			*MachineRole, NewOwner ? *NewOwner->GetName() : TEXT("nullptr"));
	}
}

// ============================================
// FIRE API IMPLEMENTATION
// ============================================

void ABaseWeapon::TriggerShoot()
{
	// Delegate to FireComponent
	if (FireComponent)
	{
		FireComponent->TriggerPulled();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::TriggerShoot() - FireComponent is NULL! Set in Blueprint."));
	}
}

void ABaseWeapon::TriggerRelease()
{
	// Delegate to FireComponent
	if (FireComponent)
	{
		FireComponent->TriggerReleased();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::TriggerRelease() - FireComponent is NULL! Set in Blueprint."));
	}
}

// ============================================
// INTERACTABLE INTERFACE IMPLEMENTATION
// ============================================

void ABaseWeapon::GetVerbs_Implementation(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const
{
	// Single verb: Pickup
	OutVerbs.Add(FPSGameplayTags::Interact_Pickup);
}

bool ABaseWeapon::CanInteract_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const
{
	// Only allow pickup if verb is correct AND weapon has no owner (not held by player)
	if (Verb == FPSGameplayTags::Interact_Pickup)
	{
		// If weapon has owner, it's being held by someone -> don't allow interaction
		return GetOwner() == nullptr;
	}

	return false;
}

void ABaseWeapon::Interact_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx)
{
	// Debug: Log interaction attempt
	FString WeaponName = Name.ToString();
	FString VerbName = Verb.ToString();
	FString PlayerName = Ctx.Pawn ? Ctx.Pawn->GetName() : TEXT("NULL");

	UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::Interact() - Weapon: %s | Verb: %s | Player: %s"),
		*WeaponName, *VerbName, *PlayerName);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("BaseWeapon::Interact() - Weapon: %s | Verb: %s | Player: %s"),
				*WeaponName, *VerbName, *PlayerName));
	}

	// Verify this is a pickup interaction
	if (Verb == FPSGameplayTags::Interact_Pickup && IPickupableInterface::Execute_CanBePicked(this, Ctx))
	{
		// Delegate to Character controller (correct architecture pattern)
		// Character handles generic pickup logic, weapon handles weapon-specific behavior
		if (Ctx.Pawn && Ctx.Pawn->IsA<AFPSCharacter>())
		{
			AFPSCharacter* Character = Cast<AFPSCharacter>(Ctx.Pawn);
			if (Character)
			{
				// Call Server RPC to pickup (multiplayer safe)
				Character->Server_PickupItem(this);
			}
		}
	}
}

FText ABaseWeapon::GetInteractionText_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const
{
	if (Verb == FPSGameplayTags::Interact_Pickup)
	{
		// Format: "Pickup [WeaponName]"
		return FText::Format(FText::FromString("Pickup {0}"), Name);
	}

	return FText::GetEmpty();
}

// ============================================
// PICKUPABLE INTERFACE IMPLEMENTATION
// ============================================

bool ABaseWeapon::CanBePicked_Implementation(const FInteractionContext& Ctx) const
{
	// Can be picked only if weapon has no owner (not held by anyone)
	return GetOwner() == nullptr;
}

void ABaseWeapon::OnPicked_Implementation(APawn* Picker, const FInteractionContext& Ctx)
{
	// WEAPON-SPECIFIC BEHAVIOR ONLY
	// Generic pickup logic (SetOwner, Attach, Physics, Inventory) is handled by FPSCharacter

	// Disable movement replication when picked up (attached actors don't need it)
	SetReplicateMovement(false);

	// Disable TPSMesh component replication when attached (character already replicates its position)
	if (TPSMesh)
	{
		TPSMesh->SetIsReplicated(false);
	}

	// Debug: Log weapon-specific pickup notification
	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnPicked() - %s notified | ReplicateMovement: false | TPSMesh replicated: false"), *Name.ToString());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			FString::Printf(TEXT("🔫 BaseWeapon::OnPicked() - %s notified"), *Name.ToString()));
	}

	// TODO: Weapon-specific pickup behavior:
	// - Play pickup sound
	// - Spawn pickup VFX
	// - Trigger pickup animation
	// - Award achievement/stat tracking
}

void ABaseWeapon::OnDropped_Implementation(const FInteractionContext& Ctx)
{
	// WEAPON-SPECIFIC BEHAVIOR ONLY
	// Generic drop logic (ClearOwner, Detach, Physics, Show) is handled by FPSCharacter

	// Enable movement replication when dropped (physics movement must replicate to all clients)
	SetReplicateMovement(true);

	// Enable TPSMesh component replication (critical for physics simulation replication)
	if (TPSMesh)
	{
		TPSMesh->SetIsReplicated(true);
	}

	// Impulse is applied by FPSCharacter:
	// - Server: FPSCharacter::DropItem() applies impulse
	// - Clients: FPSCharacter::DetachItemMeshes() applies impulse locally

	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnDropped() - %s notified | ReplicateMovement: true | TPSMesh replicated: true"), *Name.ToString());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
			FString::Printf(TEXT("🔫 BaseWeapon::OnDropped() - %s notified"), *Name.ToString()));
	}
}

// ============================================
// HOLDABLE INTERFACE IMPLEMENTATION
// ============================================

void ABaseWeapon::OnEquipped_Implementation(APawn* OwnerPawn)
{
	// WEAPON-SPECIFIC BEHAVIOR (SERVER ONLY)
	// Generic equip logic (Show, Attach, ActiveItem, Animation Linking) is handled by FPSCharacter::SetupActiveItemLocal()

	// NOTE: Magazine owner setup now happens in SetOwner() override
	// SetOwner() runs on both Server + Clients (owner replicates automatically)
	// No need to manually propagate owner here

	// TODO: Add weapon-specific equip behavior here:
	// - Play equip sound (via Multicast RPC)
	// - Play equip animation (via Multicast RPC)
	// - Initialize weapon state (ammo, fire mode)
}

void ABaseWeapon::OnUnequipped_Implementation()
{
	// ============================================
	// SERVER-SIDE GAMEPLAY STATE CLEANUP ONLY
	// ============================================
	// Visual state cleanup (mesh transforms) is handled by FPSCharacter::DetachItemMeshes()
	// which runs on ALL machines (LOCAL operation following OnRep pattern)

	// Debug: Log weapon-specific unequip notification
	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnUnequipped() - %s"), *Name.ToString());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Purple,
			FString::Printf(TEXT("🔫 BaseWeapon::OnUnequipped() - %s"), *Name.ToString()));
	}

	// ============================================
	// RESET GAMEPLAY STATE (SERVER-SIDE)
	// ============================================

	// Reset aiming state
	IsAiming = false;

	// Reset reload state
	IsReload = false;

	// NOTE: Mesh transform reset is handled by FPSCharacter::DetachItemMeshes() (LOCAL operation)
	// NOTE: Animation unlinking is handled by FPSCharacter::SetupActiveItemLocal() (LOCAL operation)

	// TODO: Weapon-specific unequip behavior (SERVER ONLY):
	// - Stop active timers (fire rate, reload, etc.)
	// - Play holster sound (via Multicast RPC)
	// - Play holster animation (via Multicast RPC)
	// - Save weapon state (ammo count, fire mode)
}

bool ABaseWeapon::IsEquipped_Implementation() const
{
	// Weapon is equipped if it has owner and is visible
	return GetOwner() != nullptr && !IsHidden();
}

FVector ABaseWeapon::GetHandsOffset_Implementation() const
{
	// Return configured hands offset for FPS arms positioning
	return HandsOffset;
}

UMeshComponent* ABaseWeapon::GetFPSMeshComponent_Implementation() const
{
	// Return FPS mesh (visible only to owner)
	return FPSMesh;
}

UMeshComponent* ABaseWeapon::GetTPSMeshComponent_Implementation() const
{
	// Return TPS mesh (root component, visible to others)
	return TPSMesh;
}

FName ABaseWeapon::GetAttachSocket_Implementation() const
{
	// Return configured character attach socket (default: "weapon_r")
	// This is the socket name on CHARACTER meshes (Arms/Body) where weapon meshes attach
	return CharacterAttachSocket;
}

TSubclassOf<UAnimInstance> ABaseWeapon::GetAnimLayer_Implementation() const
{
	// Return weapon-specific animation layer
	return AnimLayer;
}
