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

	// Dual-mesh setup: FPS and TPS as siblings to prevent transform inheritance
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(SceneRoot);
	TPSMesh->SetOwnerNoSee(true);
	TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TPSMesh->SetIsReplicated(true);
	TPSMesh->bReplicatePhysicsToAutonomousProxy = true;

	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BallisticsComponent = CreateDefaultSubobject<UBallisticsComponent>(TEXT("BallisticsComponent"));

	// Fire component - default to nullptr, will be created in Blueprint as specific subclass
	// (USemiAutoFireComponent, UFullAutoFireComponent, or UBurstFireComponent)
	FireComponent = nullptr;

	FPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("FPSMagazineComponent"));
	FPSMagazineComponent->SetupAttachment(FPSMesh, FName("magazine"));

	TPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("TPSMagazineComponent"));
	TPSMagazineComponent->SetupAttachment(TPSMesh, FName("magazine"));
}

void ABaseWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (MagazineClass)
	{
		FPSMagazineComponent->SetChildActorClass(MagazineClass);
		TPSMagazineComponent->SetChildActorClass(MagazineClass);
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Find FireComponent if it was added in Blueprint
	if (!FireComponent)
	{
		FireComponent = FindComponentByClass<UFireComponent>();
	}

	// SERVER-ONLY initialization
	if (HasAuthority() && TPSMagazineComponent->GetChildActor())
	{
		CurrentMagazine = Cast<ABaseMagazine>(TPSMagazineComponent->GetChildActor());

		if (FireComponent && BallisticsComponent)
		{
			// Set sibling component reference
			FireComponent->BallisticsComponent = BallisticsComponent;

			// Initialize ballistics with magazine's caliber type
			if (CurrentMagazine)
			{
				bool bSuccess = BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Error, TEXT("BaseWeapon::BeginPlay() - Failed to initialize ammo type for caliber: %s"),
						*UEnum::GetValueAsString(CurrentMagazine->AmmoType));
				}
			}

			UE_LOG(LogTemp, Log, TEXT("BaseWeapon::BeginPlay() - Components initialized (using interfaces)"));
		}
		else
		{
			if (!FireComponent)
			{
				UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::BeginPlay() - FireComponent not found! Add FireComponent in Blueprint."));
			}
			if (!BallisticsComponent)
			{
				UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::BeginPlay() - BallisticsComponent not found!"));
			}
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseWeapon, CurrentMagazine);
}

void ABaseWeapon::OnRep_CurrentMagazine()
{
	// TODO: Trigger UI update events
	// TODO: Play magazine change animations
	// TODO: Update HUD ammo display
}

void ABaseWeapon::SetOwner(AActor* NewOwner)
{
	Super::SetOwner(NewOwner);

	// Propagate owner to magazines
	if (FPSMagazineComponent->GetChildActor())
	{
		FPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
	}

	if (TPSMagazineComponent->GetChildActor())
	{
		TPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
	}
}

void ABaseWeapon::UseStart_Implementation(const FUseContext& Ctx)
{
	// Called locally on owning client when IA_Use pressed
	// Delegate to server via RPC
	Server_Shoot(true);
}

void ABaseWeapon::UseTick_Implementation(const FUseContext& Ctx)
{
	// Optional: continuous use while held
	// FireComponent handles fire rate internally, so this can be empty
}

void ABaseWeapon::UseStop_Implementation(const FUseContext& Ctx)
{
	// Called locally on owning client when IA_Use released
	// Delegate to server via RPC
	Server_Shoot(false);
}

void ABaseWeapon::Server_Shoot_Implementation(bool bPressed)
{
	// SERVER ONLY - runs on authority
	if (!FireComponent)
	{
		return;
	}

	if (bPressed)
	{
		// Trigger pressed - start shooting
		FireComponent->TriggerPulled();
	}
	else
	{
		// Trigger released - stop shooting
		FireComponent->TriggerReleased();
	}
}

bool ABaseWeapon::IsUsing_Implementation() const
{
	// Check if currently shooting
	if (FireComponent)
	{
		return FireComponent->IsTriggerHeld();
	}

	return false;
}

void ABaseWeapon::GetVerbs_Implementation(TArray<FGameplayTag>& OutVerbs, const FInteractionContext& Ctx) const
{
	OutVerbs.Add(FPSGameplayTags::Interact_Pickup);
}

bool ABaseWeapon::CanInteract_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const
{
	if (Verb == FPSGameplayTags::Interact_Pickup)
	{
		return GetOwner() == nullptr;
	}

	return false;
}

void ABaseWeapon::Interact_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx)
{
	if (Verb == FPSGameplayTags::Interact_Pickup && IPickupableInterface::Execute_CanBePicked(this, Ctx))
	{
		if (Ctx.Pawn && Ctx.Pawn->IsA<AFPSCharacter>())
		{
			AFPSCharacter* Character = Cast<AFPSCharacter>(Ctx.Pawn);
			if (Character)
			{
				Character->Server_PickupItem(this);
			}
		}
	}
}

FText ABaseWeapon::GetInteractionText_Implementation(FGameplayTag Verb, const FInteractionContext& Ctx) const
{
	if (Verb == FPSGameplayTags::Interact_Pickup)
	{
		return FText::Format(FText::FromString("Pickup {0}"), Name);
	}

	return FText::GetEmpty();
}

bool ABaseWeapon::CanBePicked_Implementation(const FInteractionContext& Ctx) const
{
	return GetOwner() == nullptr;
}

void ABaseWeapon::OnPicked_Implementation(APawn* Picker, const FInteractionContext& Ctx)
{
	// Generic pickup logic is handled by FPSCharacter
	TPSMesh->SetIsReplicated(false);

	// TODO: Weapon-specific pickup behavior:
	// - Play pickup sound
	// - Spawn pickup VFX
	// - Trigger pickup animation
	// - Award achievement/stat tracking
}

void ABaseWeapon::OnDropped_Implementation(const FInteractionContext& Ctx)
{
	// Generic drop logic is handled by FPSCharacter
	TPSMesh->SetIsReplicated(true);
	SetReplicateMovement(true);
}

void ABaseWeapon::OnEquipped_Implementation(APawn* OwnerPawn)
{
	// Generic equip logic is handled by FPSCharacter::SetupActiveItemLocal()

	// TODO: Add weapon-specific equip behavior here:
	// - Play equip sound (via Multicast RPC)
	// - Play equip animation (via Multicast RPC)
	// - Initialize weapon state (ammo, fire mode)
}

void ABaseWeapon::OnUnequipped_Implementation()
{
	// Visual state cleanup is handled by FPSCharacter::DetachItemMeshes()

	IsAiming = false;
	IsReload = false;

	// TODO: Weapon-specific unequip behavior (SERVER ONLY):
	// - Stop active timers (fire rate, reload, etc.)
	// - Play holster sound (via Multicast RPC)
	// - Play holster animation (via Multicast RPC)
	// - Save weapon state (ammo count, fire mode)
}

bool ABaseWeapon::IsEquipped_Implementation() const
{
	return GetOwner() != nullptr && !IsHidden();
}

FVector ABaseWeapon::GetHandsOffset_Implementation() const
{
	return HandsOffset;
}

UMeshComponent* ABaseWeapon::GetFPSMeshComponent_Implementation() const
{
	return FPSMesh;
}

UMeshComponent* ABaseWeapon::GetTPSMeshComponent_Implementation() const
{
	return TPSMesh;
}

FName ABaseWeapon::GetAttachSocket_Implementation() const
{
	return CharacterAttachSocket;
}

TSubclassOf<UAnimInstance> ABaseWeapon::GetAnimLayer_Implementation() const
{
	return AnimLayer;
}

// ============================================
// BALLISTICS HANDLER INTERFACE IMPLEMENTATION
// ============================================

void ABaseWeapon::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// SERVER ONLY
	if (HasAuthority())
	{
		// Replicate to all clients via Multicast RPC
		Multicast_PlayMuzzleFlash(MuzzleLocation, Direction);
	}
}

void ABaseWeapon::HandleImpactDetected_Implementation(
	const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX,
	FVector_NetQuantize Location,
	FVector_NetQuantizeNormal Normal)
{
	// SERVER ONLY
	if (HasAuthority())
	{
		// Replicate to all clients via Multicast RPC
		Multicast_SpawnImpactEffect(ImpactVFX, Location, Normal);
	}
}

// ============================================
// MUZZLE EFFECTS (Multiplayer)
// ============================================

void ABaseWeapon::Multicast_PlayMuzzleFlash_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	// Runs on SERVER and ALL CLIENTS

	// 1. Spawn muzzle flash VFX attached to weapon barrel bone
	if (MuzzleFlashNiagara)
	{
		// Determine which mesh to use based on ownership
		USkeletalMeshComponent* MuzzleMesh = nullptr;

		// Check if local player is owner
		APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			MuzzleMesh = FPSMesh; // Owner sees FPS mesh
			UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::Multicast_PlayMuzzleFlash() - Using FPS mesh (owner)"));
		}
		else
		{
			MuzzleMesh = TPSMesh; // Others see TPS mesh
			UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::Multicast_PlayMuzzleFlash() - Using TPS mesh (spectator)"));
		}

		if (MuzzleMesh)
		{
			// ✅ ATTACH to "barrel" bone (not spawn at location!)
			UNiagaraComponent* MuzzleFlashComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				MuzzleFlashNiagara,
				MuzzleMesh,
				FName("barrel"),           // Attach to barrel bone
				FVector::ZeroVector,       // No offset
				FRotator::ZeroRotator,     // No rotation offset
				EAttachLocation::SnapToTarget, // Snap to bone transform
				true,                      // Auto destroy
				true,                      // Auto activate
				ENCPoolMethod::None
			);

			if (MuzzleFlashComponent)
			{
				UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::Multicast_PlayMuzzleFlash() - Muzzle flash attached to barrel bone"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::Multicast_PlayMuzzleFlash() - Failed to spawn muzzle flash! Check MuzzleFlashNiagara asset."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BaseWeapon::Multicast_PlayMuzzleFlash() - MuzzleMesh is null! Weapon not properly initialized."));
		}
	}

	// 2. Play shoot sound (TODO: Add USoundCue property)
	// if (ShootSound)
	// {
	//     UGameplayStatics::PlaySoundAtLocation(GetWorld(), ShootSound, MuzzleLocation);
	// }

	// 3. Play shoot animation (TODO: Implement)
	// if (ShootMontage && GetOwner())
	// {
	//     APawn* OwnerPawn = Cast<APawn>(GetOwner());
	//     UAnimInstance* AnimInstance = OwnerPawn->GetMesh()->GetAnimInstance();
	//     if (AnimInstance)
	//     {
	//         AnimInstance->Montage_Play(ShootMontage);
	//     }
	// }

	// 4. Shell ejection (TODO: Implement)
}

void ABaseWeapon::Multicast_SpawnImpactEffect_Implementation(
	const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX,
	FVector_NetQuantize Location,
	FVector_NetQuantizeNormal Normal)
{
	// Runs on SERVER and ALL CLIENTS
	// Load VFX asset (synchronous for simplicity)
	UNiagaraSystem* VFX = ImpactVFX.LoadSynchronous();

	if (VFX)
	{
		// Spawn Niagara system at impact point
		// ✅ Create rotation from surface normal (Z-axis aligned with normal)
		// This ensures effect spawns perpendicular to surface (correct for decals, dust, sparks)
		FRotator ImpactRotation = FRotationMatrix::MakeFromZ(Normal).Rotator();

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			VFX,
			Location,
			ImpactRotation,
			FVector(1.0f),  // Scale
			true,           // Auto destroy
			true,           // Auto activate
			ENCPoolMethod::None
		);

		UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::Multicast_SpawnImpactEffect() - Impact VFX spawned at %s, Normal: %s, Rotation: %s"),
			*Location.ToString(), *Normal.ToString(), *ImpactRotation.ToString());
	}
}

// ============================================
// AMMO CONSUMER INTERFACE IMPLEMENTATION
// ============================================

FName ABaseWeapon::GetAmmoType_Implementation() const
{
	if (CurrentMagazine)
	{
		return FName(*UEnum::GetValueAsString(CurrentMagazine->AmmoType));
	}
	return NAME_None;
}

int32 ABaseWeapon::GetClip_Implementation() const
{
	return CurrentMagazine ? CurrentMagazine->CurrentAmmo : 0;
}

int32 ABaseWeapon::GetClipSize_Implementation() const
{
	return CurrentMagazine ? CurrentMagazine->MaxCapacity : 0;
}

int32 ABaseWeapon::GetTotalAmmo_Implementation() const
{
	// TODO: Implement reserve ammo system
	return 0;
}

int32 ABaseWeapon::ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx)
{
	// SERVER ONLY
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BaseWeapon::ConsumeAmmo() - Called on CLIENT! This should only run on server."));
		return 0;
	}

	if (!CurrentMagazine || CurrentMagazine->CurrentAmmo <= 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::ConsumeAmmo() - No ammo available"));
		return 0;
	}

	// Check if reloading
	if (IsReload)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::ConsumeAmmo() - Weapon is reloading"));
		return 0;
	}

	// Consume requested amount (or all remaining if less available)
	int32 AmmoToConsume = FMath::Min(Requested, CurrentMagazine->CurrentAmmo);

	for (int32 i = 0; i < AmmoToConsume; i++)
	{
		CurrentMagazine->RemoveAmmo();
	}

	UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::ConsumeAmmo() - Consumed %d rounds, remaining: %d"),
		AmmoToConsume, CurrentMagazine->CurrentAmmo);

	return AmmoToConsume;
}
