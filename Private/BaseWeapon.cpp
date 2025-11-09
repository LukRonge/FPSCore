// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Core/FPSGameplayTags.h"
#include "FPSCharacter.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// Create root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create third-person mesh (visible to everyone except owner)
	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(RootComponent);
	TPSMesh->SetOnlyOwnerSee(false);
	TPSMesh->SetOwnerNoSee(true);

	// Create first-person mesh (visible only to owner)
	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(RootComponent);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetOwnerNoSee(false);
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Only magazine reference is replicated
	DOREPLIFETIME(ABaseWeapon, CurrentMagazine);
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

	// Debug: Log weapon-specific pickup notification
	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnPicked() - %s notified"), *Name.ToString());

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

	// Debug: Log weapon-specific drop notification
	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnDropped() - %s notified"), *Name.ToString());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
			FString::Printf(TEXT("🔫 BaseWeapon::OnDropped() - %s notified"), *Name.ToString()));
	}

	// Apply throw impulse to TPS mesh (visible when dropped)
	if (TPSMesh && Ctx.Pawn)
	{
		// Calculate throw direction (forward + up arc)
		FVector ThrowDirection = Ctx.Pawn->GetActorForwardVector() + FVector(0, 0, 0.5f);
		ThrowDirection.Normalize();

		FVector Impulse = ThrowDirection * DropImpulseStrength;
		TPSMesh->AddImpulse(Impulse, NAME_None, true);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
				FString::Printf(TEXT("✓ Applied drop impulse: %s"), *Impulse.ToString()));
		}
	}
}

// ============================================
// HOLDABLE INTERFACE IMPLEMENTATION
// ============================================

void ABaseWeapon::OnEquipped_Implementation(APawn* OwnerPawn)
{
	// WEAPON-SPECIFIC BEHAVIOR ONLY
	// Generic equip logic (Show, Attach, ActiveItem, Animation Linking) is handled by FPSCharacter::SetupActiveItemLocal()

	// Debug: Log weapon-specific equip notification
	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnEquipped() - %s"), *Name.ToString());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Magenta,
			FString::Printf(TEXT("🔫 BaseWeapon::OnEquipped() - %s"), *Name.ToString()));
	}

	// NOTE: Animation linking is now handled by FPSCharacter::SetupActiveItemLocal()
	// which runs on ALL machines (server + clients) following MULTIPLAYER_GUIDELINES.md OnRep pattern

	// TODO: Weapon-specific equip behavior (SERVER ONLY):
	// - Play equip sound (via Multicast RPC)
	// - Play equip animation (via Multicast RPC)
	// - Initialize weapon state (ammo, fire mode)
}

void ABaseWeapon::OnUnequipped_Implementation()
{
	// WEAPON-SPECIFIC BEHAVIOR ONLY
	// Generic unequip logic (Hide, ClearActiveItem, Animation Unlinking) is handled by FPSCharacter::SetupActiveItemLocal()

	// Debug: Log weapon-specific unequip notification
	UE_LOG(LogTemp, Warning, TEXT("🔫 BaseWeapon::OnUnequipped() - %s"), *Name.ToString());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Purple,
			FString::Printf(TEXT("🔫 BaseWeapon::OnUnequipped() - %s"), *Name.ToString()));
	}

	// NOTE: Animation unlinking is now handled by FPSCharacter::SetupActiveItemLocal()
	// when ActiveItem changes or becomes null (unarmed state)

	// TODO: Weapon-specific unequip behavior (SERVER ONLY):
	// - Play holster sound (via Multicast RPC)
	// - Play holster animation (via Multicast RPC)
	// - Save weapon state
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
	// Return FPS skeletal mesh (visible only to owner)
	return FPSMesh;
}

UMeshComponent* ABaseWeapon::GetTPSMeshComponent_Implementation() const
{
	// Return TPS skeletal mesh (visible to everyone except owner)
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
