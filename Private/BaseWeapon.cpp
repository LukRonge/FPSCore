// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeapon.h"
#include "Components/BallisticsComponent.h"
#include "Components/FireComponent.h"
#include "Components/ReloadComponent.h"
#include "Net/UnrealNetwork.h"
#include "Core/FPSGameplayTags.h"
#include "BaseMagazine.h"
#include "BaseSight.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/ItemWidgetProviderInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoProviderInterface.h"
#include "Interfaces/MagazineMeshProviderInterface.h"
#include "Interfaces/SightMeshProviderInterface.h"
#include "NiagaraComponent.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(SceneRoot);
	TPSMesh->SetOwnerNoSee(true);
	TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TPSMesh->SetSimulatePhysics(true);
	TPSMesh->SetIsReplicated(true);
	TPSMesh->bReplicatePhysicsToAutonomousProxy = true;

	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FPSMesh->PrimaryComponentTick.bCanEverTick = false;

	BallisticsComponent = CreateDefaultSubobject<UBallisticsComponent>(TEXT("BallisticsComponent"));
	FireComponent = nullptr;

	// Single magazine component - attached to TPSMesh "magazine" socket
	// Magazine actor has its own FPS/TPS meshes with appropriate visibility
	MagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("MagazineComponent"));
	MagazineComponent->SetupAttachment(TPSMesh, FName("magazine"));
	MagazineComponent->SetIsReplicated(false);  // CurrentMagazine (actor pointer) is replicated instead

	// Single sight component - initially attached to FPSMesh without socket
	// Socket is set in InitSightComponents() after sight actor is created (socket name comes from sight)
	SightComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("SightComponent"));
	SightComponent->SetupAttachment(FPSMesh);
	SightComponent->SetIsReplicated(false);  // CurrentSight (actor pointer) is replicated instead
}

void ABaseWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] PostInitializeComponents - %s | HasAuthority=%d | NetMode=%d | Owner=%s"),
		*GetName(),
		HasAuthority(),
		(int32)GetNetMode(),
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));

	// ============================================
	// COMPONENT INITIALIZATION (ALL MACHINES)
	// ============================================
	// Find components that may be added via Blueprint
	// This runs before BeginPlay, ensuring components are available

	if (!FireComponent)
	{
		FireComponent = FindComponentByClass<UFireComponent>();
	}

	if (!ReloadComponent)
	{
		ReloadComponent = FindComponentByClass<UReloadComponent>();
	}

	// ============================================
	// SERVER ONLY: Initialize magazine and sight components
	// ============================================
	// Server creates child actors and sets CurrentMagazine/CurrentSight (REPLICATED)
	// Remote clients receive actor pointers via OnRep_CurrentMagazine / OnRep_CurrentSight

	if (HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] PostInitializeComponents SERVER - Initializing Magazine=%s, Sight=%s"),
			DefaultMagazineClass ? *DefaultMagazineClass->GetName() : TEXT("nullptr"),
			DefaultSightClass ? *DefaultSightClass->GetName() : TEXT("nullptr"));

		InitMagazineComponents(DefaultMagazineClass);
		InitSightComponents(DefaultSightClass);
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] BeginPlay - %s | HasAuthority=%d | NetMode=%d | Owner=%s"),
		*GetName(),
		HasAuthority(),
		(int32)GetNetMode(),
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));

	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] BeginPlay - CurrentMagazine=%s | CurrentSight=%s"),
		CurrentMagazine ? *CurrentMagazine->GetName() : TEXT("nullptr"),
		CurrentSight ? *CurrentSight->GetName() : TEXT("nullptr"));

	// ============================================
	// CLIENT NOTE: Magazine and Sight actors are received via replication
	// OnRep_CurrentMagazine and OnRep_CurrentSight handle attachment
	// No client-side initialization needed here
	// ============================================

	// ============================================
	// SERVER ONLY: Link FireComponent to BallisticsComponent
	// ============================================
	if (HasAuthority() && FireComponent && BallisticsComponent)
	{
		FireComponent->BallisticsComponent = BallisticsComponent;

		// Initialize ballistics with magazine's ammo type
		// CurrentMagazine is set in InitMagazineComponents (PostInitializeComponents)
		if (CurrentMagazine)
		{
			BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate actor pointers ONLY - not class references
	// This eliminates race conditions where OnRep_Class fires before actor exists
	// Client receives actor via replication → OnRep handles local attachment
	DOREPLIFETIME(ABaseWeapon, CurrentMagazine);
	DOREPLIFETIME(ABaseWeapon, CurrentSight);
}

void ABaseWeapon::OnRep_CurrentMagazine()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnRep_CurrentMagazine - %s | CurrentMagazine=%s | NetMode=%d"),
		*GetName(),
		CurrentMagazine ? *CurrentMagazine->GetName() : TEXT("nullptr"),
		(int32)GetNetMode());

	// CLIENT ONLY - called when CurrentMagazine actor is replicated from server
	if (CurrentMagazine)
	{
		// Synchronize BallisticsComponent with magazine's ammo type
		if (BallisticsComponent)
		{
			BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
		}

		// Propagate owner for correct visibility (OnlyOwnerSee/OwnerNoSee)
		CurrentMagazine->SetOwner(GetOwner());

		// Attach magazine meshes to weapon meshes (LOCAL operation)
		AttachMagazineMeshes();
	}
}

void ABaseWeapon::SetOwner(AActor* NewOwner)
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] SetOwner - %s | NewOwner=%s | HasAuthority=%d | NetMode=%d"),
		*GetName(),
		NewOwner ? *NewOwner->GetName() : TEXT("nullptr"),
		HasAuthority(),
		(int32)GetNetMode());

	Super::SetOwner(NewOwner);
	PropagateOwnerToChildActors(NewOwner);
}

void ABaseWeapon::OnRep_Owner()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnRep_Owner - %s | Owner=%s | NetMode=%d | HasBegunPlay=%d"),
		*GetName(),
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"),
		(int32)GetNetMode(),
		HasActorBegunPlay());

	Super::OnRep_Owner();
	PropagateOwnerToChildActors(GetOwner());
}

void ABaseWeapon::PropagateOwnerToChildActors(AActor* NewOwner)
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] PropagateOwnerToChildActors - %s | NewOwner=%s"),
		*GetName(),
		NewOwner ? *NewOwner->GetName() : TEXT("nullptr"));

	// Propagate owner to magazine (use replicated actor pointer)
	if (CurrentMagazine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon]   -> Magazine %s | OldOwner=%s | NewOwner=%s"),
			*CurrentMagazine->GetName(),
			CurrentMagazine->GetOwner() ? *CurrentMagazine->GetOwner()->GetName() : TEXT("nullptr"),
			NewOwner ? *NewOwner->GetName() : TEXT("nullptr"));
		CurrentMagazine->SetOwner(NewOwner);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon]   -> Magazine NOT FOUND (CurrentMagazine=nullptr)"));
	}

	// Propagate owner to sight (use replicated actor pointer)
	if (CurrentSight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon]   -> Sight %s | OldOwner=%s | NewOwner=%s"),
			*CurrentSight->GetName(),
			CurrentSight->GetOwner() ? *CurrentSight->GetOwner()->GetName() : TEXT("nullptr"),
			NewOwner ? *NewOwner->GetName() : TEXT("nullptr"));
		CurrentSight->SetOwner(NewOwner);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon]   -> Sight NOT FOUND (CurrentSight=nullptr)"));
	}

	// NOTE: No re-attachment needed - meshes are attached once in Init*Components() (server)
	// or OnRep_CurrentMagazine/OnRep_CurrentSight (client)
	// Visibility is handled natively by engine via render flags (OnlyOwnerSee/OwnerNoSee)
}

void ABaseWeapon::UseStart_Implementation(const FUseContext& Ctx)
{
	Server_Shoot(true);
}

void ABaseWeapon::UseTick_Implementation(const FUseContext& Ctx)
{
}

void ABaseWeapon::UseStop_Implementation(const FUseContext& Ctx)
{
	Server_Shoot(false);
}

void ABaseWeapon::Server_Shoot_Implementation(bool bPressed)
{
	if (!FireComponent) return;

	if (bPressed)
	{
		FireComponent->TriggerPulled();
	}
	else
	{
		FireComponent->TriggerReleased();
	}
}

bool ABaseWeapon::IsUsing_Implementation() const
{
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
		if (GetOwner() == nullptr && Ctx.Pawn && Ctx.Pawn->Implements<UItemCollectorInterface>())
		{
			IItemCollectorInterface::Execute_Pickup(Ctx.Pawn, this);
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
}

void ABaseWeapon::OnDropped_Implementation(const FInteractionContext& Ctx)
{
	SetReplicateMovement(true);
}

void ABaseWeapon::OnEquipped_Implementation(APawn* OwnerPawn)
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnEquipped - %s | OwnerPawn=%s | CurrentOwner=%s | HasAuthority=%d | NetMode=%d"),
		*GetName(),
		OwnerPawn ? *OwnerPawn->GetName() : TEXT("nullptr"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"),
		HasAuthority(),
		(int32)GetNetMode());

	// Log magazine and sight owners (use replicated pointers)
	if (CurrentMagazine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnEquipped - Magazine %s Owner=%s"),
			*CurrentMagazine->GetName(),
			CurrentMagazine->GetOwner() ? *CurrentMagazine->GetOwner()->GetName() : TEXT("nullptr"));
	}
	if (CurrentSight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnEquipped - Sight %s Owner=%s"),
			*CurrentSight->GetName(),
			CurrentSight->GetOwner() ? *CurrentSight->GetOwner()->GetName() : TEXT("nullptr"));
	}
}

void ABaseWeapon::OnUnequipped_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnUnequipped - %s | CurrentOwner=%s | HasAuthority=%d | NetMode=%d"),
		*GetName(),
		GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"),
		HasAuthority(),
		(int32)GetNetMode());

	IsAiming = false;

	if (ReloadComponent && ReloadComponent->bIsReloading)
	{
		ReloadComponent->Server_CancelReload();
	}
}

bool ABaseWeapon::IsEquipped_Implementation() const
{
	return GetOwner() != nullptr && !IsHidden();
}

FVector ABaseWeapon::GetArmsOffset_Implementation() const
{
	return ArmsOffset;
}

UPrimitiveComponent* ABaseWeapon::GetFPSMeshComponent_Implementation() const
{
	return FPSMesh;
}

UPrimitiveComponent* ABaseWeapon::GetTPSMeshComponent_Implementation() const
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

float ABaseWeapon::GetLeaningScale_Implementation() const
{
	return LeaningScale;
}

float ABaseWeapon::GetBreathingScale_Implementation() const
{
	return BreathingScale;
}

void ABaseWeapon::HandleShotFired_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	if (HasAuthority())
	{
		// Single Multicast handles all visual effects
		// Each client locally determines what to render based on IsLocallyControlled()
		Multicast_PlayShootEffects();
	}
}

void ABaseWeapon::HandleImpactDetected_Implementation(
	const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX,
	FVector_NetQuantize Location,
	FVector_NetQuantizeNormal Normal)
{
	if (HasAuthority())
	{
		Multicast_SpawnImpactEffect(ImpactVFX, Location, Normal);
	}
}

void ABaseWeapon::SpawnMuzzleFlashOnMesh(USkeletalMeshComponent* Mesh, bool bIsFirstPerson)
{
	if (!MuzzleFlashNiagara || !Mesh)
	{
		return;
	}

	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		MuzzleFlashNiagara,
		Mesh,
		FName("barrel"),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true,
		true,
		ENCPoolMethod::None
	);

	// CRITICAL: Niagara components do NOT inherit visibility from parent mesh
	// Must explicitly set visibility flags to match the mesh
	if (NiagaraComp)
	{
		if (bIsFirstPerson)
		{
			// FPS: Only owner sees this
			NiagaraComp->SetOnlyOwnerSee(true);
			NiagaraComp->SetOwnerNoSee(false);
		}
		else
		{
			// TPS: Everyone except owner sees this
			NiagaraComp->SetOnlyOwnerSee(false);
			NiagaraComp->SetOwnerNoSee(true);
		}
	}
}

void ABaseWeapon::Multicast_PlayShootEffects_Implementation()
{
	// ============================================
	// STEP 1: EARLY OUT FOR DEDICATED SERVER
	// ============================================
	// Dedicated servers don't render - skip particle spawning
	// BUT keep animations running for physics/hitbox updates
	const bool bIsDedicatedServer = (GetNetMode() == NM_DedicatedServer);

	// ============================================
	// STEP 2: DETERMINE VIEW PERSPECTIVE
	// ============================================
	AActor* WeaponOwner = GetOwner();
	APawn* OwnerPawn = WeaponOwner ? Cast<APawn>(WeaponOwner) : nullptr;
	const bool bIsLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();

	// ============================================
	// STEP 3: MUZZLE FLASH VFX (Skip on dedicated server)
	// ============================================
	if (!bIsDedicatedServer)
	{
		if (bIsLocallyControlled)
		{
			// FPS VIEW: Spawn on FPSMesh (OnlyOwnerSee)
			SpawnMuzzleFlashOnMesh(FPSMesh, true);
		}
		else
		{
			// TPS VIEW: Spawn on TPSMesh (OwnerNoSee)
			SpawnMuzzleFlashOnMesh(TPSMesh, false);
		}
	}

	// ============================================
	// STEP 4: SHOOT ANIMATIONS
	// ============================================
	// Animations run on ALL machines (including dedicated server)
	// Reason: Animation drives physics, hitboxes, IK, and gameplay state
	// Visibility is handled by mesh settings (OwnerNoSee, OnlyOwnerSee)

	if (!ShootMontage || !WeaponOwner)
	{
		return;
	}

	if (!WeaponOwner->Implements<UCharacterMeshProviderInterface>())
	{
		return;
	}

	// --- FPS ANIMATIONS (Owning client only) ---
	// Arms mesh has OnlyOwnerSee, so only owning client sees this
	if (bIsLocallyControlled)
	{
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(WeaponOwner);
		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(ShootMontage);
			}
		}
	}

	// --- TPS ANIMATIONS (All machines for remote view) ---
	// Body/Legs have OwnerNoSee, so owning client won't see them rendering
	// But animation still plays for physics synchronization and remote clients
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(WeaponOwner);
	if (BodyMesh)
	{
		if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(ShootMontage);
		}
	}

	USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(WeaponOwner);
	if (LegsMesh)
	{
		if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(ShootMontage);
		}
	}
}

void ABaseWeapon::Multicast_SpawnImpactEffect_Implementation(
	const TSoftObjectPtr<UNiagaraSystem>& ImpactVFX,
	FVector_NetQuantize Location,
	FVector_NetQuantizeNormal Normal)
{
	UNiagaraSystem* VFX = ImpactVFX.LoadSynchronous();

	if (VFX)
	{
		FRotator ImpactRotation = FRotationMatrix::MakeFromZ(Normal).Rotator();

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			VFX,
			Location,
			ImpactRotation,
			FVector(1.0f),
			true,
			true,
			ENCPoolMethod::None
		);
	}
}

FName ABaseWeapon::GetAmmoType_Implementation() const
{
	if (CurrentMagazine && CurrentMagazine->Implements<UAmmoProviderInterface>())
	{
		return IAmmoProviderInterface::Execute_GetAmmoType(CurrentMagazine);
	}
	return NAME_None;
}

int32 ABaseWeapon::GetClip_Implementation() const
{
	if (CurrentMagazine && CurrentMagazine->Implements<UAmmoProviderInterface>())
	{
		return IAmmoProviderInterface::Execute_GetCurrentAmmo(CurrentMagazine);
	}
	return 0;
}

int32 ABaseWeapon::GetClipSize_Implementation() const
{
	if (CurrentMagazine && CurrentMagazine->Implements<UAmmoProviderInterface>())
	{
		return IAmmoProviderInterface::Execute_GetMaxCapacity(CurrentMagazine);
	}
	return 0;
}

int32 ABaseWeapon::GetTotalAmmo_Implementation() const
{
	return 0;
}

int32 ABaseWeapon::ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx)
{
	if (!HasAuthority()) return 0;
	if (!CurrentMagazine || !CurrentMagazine->Implements<UAmmoProviderInterface>()) return 0;
	if (ReloadComponent && ReloadComponent->bIsReloading) return 0;

	// Validate requested amount
	if (Requested <= 0) return 0;

	// Get current ammo via interface
	int32 CurrentAmmo = IAmmoProviderInterface::Execute_GetCurrentAmmo(CurrentMagazine);
	if (CurrentAmmo <= 0) return 0;

	int32 AmmoToConsume = FMath::Min(Requested, CurrentAmmo);

	// Single ammo update instead of loop - reduces network traffic
	// CurrentMagazine->CurrentAmmo is REPLICATED, so this triggers one replication
	IAmmoProviderInterface::Execute_SetCurrentAmmo(CurrentMagazine, FMath::Max(0, CurrentAmmo - AmmoToConsume));

	return AmmoToConsume;
}

void ABaseWeapon::InitSightComponents(TSubclassOf<ABaseSight> SightClass)
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitSightComponents - %s | SightClass=%s | HasAuthority=%d | NetMode=%d"),
		*GetName(),
		SightClass ? *SightClass->GetName() : TEXT("nullptr"),
		HasAuthority(),
		(int32)GetNetMode());

	// SERVER ONLY - clients receive CurrentSight via replication
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitSightComponents - SKIPPED (not authority, will receive via replication)"));
		return;
	}

	if (!SightComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] InitSightComponents - SightComponent is nullptr!"));
		return;
	}

	if (SightClass)
	{
		// Create child actor
		SightComponent->SetChildActorClass(TSubclassOf<AActor>(SightClass));

		AActor* SightActor = SightComponent->GetChildActor();
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitSightComponents - ChildActor=%s"),
			SightActor ? *SightActor->GetName() : TEXT("nullptr"));

		if (SightActor)
		{
			// Set authoritative sight reference (REPLICATED to clients)
			CurrentSight = Cast<ABaseSight>(SightActor);
			UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitSightComponents - SERVER set CurrentSight=%s"),
				CurrentSight ? *CurrentSight->GetName() : TEXT("nullptr"));

			// Propagate owner for correct visibility
			UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitSightComponents - Propagating Owner=%s to SightActor"),
				GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));
			SightActor->SetOwner(GetOwner());

			// Attach sight meshes to weapon meshes (SERVER does this locally too)
			AttachSightMeshes();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] InitSightComponents - SetChildActorClass succeeded but GetChildActor returned nullptr!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitSightComponents - Clearing sight (SightClass is nullptr)"));
		SightComponent->SetChildActorClass(nullptr);
		CurrentSight = nullptr;
	}
}

void ABaseWeapon::AttachSightMeshes()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachSightMeshes - %s | CurrentSight=%s"),
		*GetName(),
		CurrentSight ? *CurrentSight->GetName() : TEXT("nullptr"));

	// Use CurrentSight (replicated actor pointer) instead of SightComponent->GetChildActor()
	if (!CurrentSight)
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachSightMeshes - CurrentSight is nullptr!"));
		return;
	}

	if (!CurrentSight->Implements<USightMeshProviderInterface>())
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachSightMeshes - CurrentSight does NOT implement USightMeshProviderInterface!"));
		return;
	}

	UPrimitiveComponent* FPSSightMesh = ISightMeshProviderInterface::Execute_GetFPSMesh(CurrentSight);
	UPrimitiveComponent* TPSSightMesh = ISightMeshProviderInterface::Execute_GetTPSMesh(CurrentSight);
	FName AttachSocket = ISightMeshProviderInterface::Execute_GetAttachSocket(CurrentSight);

	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachSightMeshes - FPSSightMesh=%s | TPSSightMesh=%s | AttachSocket=%s"),
		FPSSightMesh ? *FPSSightMesh->GetName() : TEXT("nullptr"),
		TPSSightMesh ? *TPSSightMesh->GetName() : TEXT("nullptr"),
		*AttachSocket.ToString());

	// FPS sight mesh -> FPS weapon mesh (visible only to owner)
	if (FPSSightMesh && FPSMesh)
	{
		FPSSightMesh->AttachToComponent(
			FPSMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachSocket
		);

		USceneComponent* AttachParent = FPSSightMesh->GetAttachParent();
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachSightMeshes - FPSSightMesh attached to %s at socket %s"),
			AttachParent ? *AttachParent->GetName() : TEXT("nullptr"),
			*FPSSightMesh->GetAttachSocketName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachSightMeshes - FPSSightMesh=%s FPSMesh=%s - CANNOT ATTACH FPS!"),
			FPSSightMesh ? TEXT("valid") : TEXT("nullptr"),
			FPSMesh ? TEXT("valid") : TEXT("nullptr"));
	}

	// TPS sight mesh -> TPS weapon mesh (visible to others)
	if (TPSSightMesh && TPSMesh)
	{
		TPSSightMesh->AttachToComponent(
			TPSMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachSocket
		);

		USceneComponent* AttachParent = TPSSightMesh->GetAttachParent();
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachSightMeshes - TPSSightMesh attached to %s at socket %s"),
			AttachParent ? *AttachParent->GetName() : TEXT("nullptr"),
			*TPSSightMesh->GetAttachSocketName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachSightMeshes - TPSSightMesh=%s TPSMesh=%s - CANNOT ATTACH TPS!"),
			TPSSightMesh ? TEXT("valid") : TEXT("nullptr"),
			TPSMesh ? TEXT("valid") : TEXT("nullptr"));
	}
}

void ABaseWeapon::OnRep_CurrentSight()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] OnRep_CurrentSight - %s | CurrentSight=%s | NetMode=%d"),
		*GetName(),
		CurrentSight ? *CurrentSight->GetName() : TEXT("nullptr"),
		(int32)GetNetMode());

	// CLIENT ONLY - called when CurrentSight actor is replicated from server
	if (CurrentSight)
	{
		// Propagate owner for correct visibility (OnlyOwnerSee/OwnerNoSee)
		CurrentSight->SetOwner(GetOwner());

		// Attach sight meshes to weapon meshes (LOCAL operation)
		AttachSightMeshes();
	}
}

void ABaseWeapon::InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass)
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitMagazineComponents - %s | MagazineClass=%s | HasAuthority=%d | NetMode=%d"),
		*GetName(),
		MagazineClass ? *MagazineClass->GetName() : TEXT("nullptr"),
		HasAuthority(),
		(int32)GetNetMode());

	// SERVER ONLY - clients receive CurrentMagazine via replication
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitMagazineComponents - SKIPPED (not authority, will receive via replication)"));
		return;
	}

	if (!MagazineComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] InitMagazineComponents - MagazineComponent is nullptr!"));
		return;
	}

	if (MagazineClass)
	{
		// Create child actor
		MagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));

		AActor* MagActor = MagazineComponent->GetChildActor();
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitMagazineComponents - ChildActor=%s"),
			MagActor ? *MagActor->GetName() : TEXT("nullptr"));

		if (MagActor)
		{
			// Set authoritative magazine reference (REPLICATED to clients)
			CurrentMagazine = Cast<ABaseMagazine>(MagActor);
			UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitMagazineComponents - SERVER set CurrentMagazine=%s"),
				CurrentMagazine ? *CurrentMagazine->GetName() : TEXT("nullptr"));

			// Propagate owner for correct visibility
			UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitMagazineComponents - Propagating Owner=%s to MagActor"),
				GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));
			MagActor->SetOwner(GetOwner());

			// Attach magazine meshes to weapon meshes (SERVER does this locally too)
			AttachMagazineMeshes();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] InitMagazineComponents - SetChildActorClass succeeded but GetChildActor returned nullptr!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] InitMagazineComponents - Clearing magazine (MagazineClass is nullptr)"));
		MagazineComponent->SetChildActorClass(nullptr);
		CurrentMagazine = nullptr;
	}
}

void ABaseWeapon::AttachMagazineMeshes()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachMagazineMeshes - %s | CurrentMagazine=%s"),
		*GetName(),
		CurrentMagazine ? *CurrentMagazine->GetName() : TEXT("nullptr"));

	// Use CurrentMagazine (replicated actor pointer) instead of MagazineComponent->GetChildActor()
	if (!CurrentMagazine)
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachMagazineMeshes - CurrentMagazine is nullptr!"));
		return;
	}

	if (!CurrentMagazine->Implements<UMagazineMeshProviderInterface>())
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachMagazineMeshes - CurrentMagazine does NOT implement UMagazineMeshProviderInterface!"));
		return;
	}

	UPrimitiveComponent* FPSMagMesh = IMagazineMeshProviderInterface::Execute_GetFPSMesh(CurrentMagazine);
	UPrimitiveComponent* TPSMagMesh = IMagazineMeshProviderInterface::Execute_GetTPSMesh(CurrentMagazine);

	UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachMagazineMeshes - FPSMagMesh=%s | TPSMagMesh=%s"),
		FPSMagMesh ? *FPSMagMesh->GetName() : TEXT("nullptr"),
		TPSMagMesh ? *TPSMagMesh->GetName() : TEXT("nullptr"));

	// FPS magazine mesh -> FPS weapon mesh (visible only to owner)
	if (FPSMagMesh && FPSMesh)
	{
		FPSMagMesh->AttachToComponent(
			FPSMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			FName("magazine")
		);

		USceneComponent* AttachParent = FPSMagMesh->GetAttachParent();
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachMagazineMeshes - FPSMagMesh attached to %s at socket %s"),
			AttachParent ? *AttachParent->GetName() : TEXT("nullptr"),
			*FPSMagMesh->GetAttachSocketName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachMagazineMeshes - FPSMagMesh=%s FPSMesh=%s - CANNOT ATTACH FPS!"),
			FPSMagMesh ? TEXT("valid") : TEXT("nullptr"),
			FPSMesh ? TEXT("valid") : TEXT("nullptr"));
	}

	// TPS magazine mesh -> TPS weapon mesh (visible to others)
	if (TPSMagMesh && TPSMesh)
	{
		TPSMagMesh->AttachToComponent(
			TPSMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			FName("magazine")
		);

		USceneComponent* AttachParent = TPSMagMesh->GetAttachParent();
		UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] AttachMagazineMeshes - TPSMagMesh attached to %s at socket %s"),
			AttachParent ? *AttachParent->GetName() : TEXT("nullptr"),
			*TPSMagMesh->GetAttachSocketName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BaseWeapon] AttachMagazineMeshes - TPSMagMesh=%s TPSMesh=%s - CANNOT ATTACH TPS!"),
			TPSMagMesh ? TEXT("valid") : TEXT("nullptr"),
			TPSMesh ? TEXT("valid") : TEXT("nullptr"));
	}
}

TSubclassOf<UUserWidget> ABaseWeapon::GetCrossHair_Implementation() const
{
	return CrossHair;
}

AActor* ABaseWeapon::GetCurrentSightActor() const
{
	// Use replicated CurrentSight pointer instead of SightComponent->GetChildActor()
	// This ensures clients have correct sight reference even before component is ready
	return CurrentSight;
}

TSubclassOf<UUserWidget> ABaseWeapon::GetAimingCrosshair_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimingCrosshair(SightActor);
	}
	return nullptr;
}

FVector ABaseWeapon::GetAimingPoint_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimingPoint(SightActor);
	}
	return DefaultAimPoint;
}

AActor* ABaseWeapon::GetSightActor_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetSightActor(SightActor);
	}
	return nullptr;
}

float ABaseWeapon::GetAimingFOV_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		float SightFOV = ISightInterface::Execute_GetAimingFOV(SightActor);
		if (SightFOV > 0.0f)
		{
			return SightFOV;
		}
	}
	return AimFOV;
}

float ABaseWeapon::GetAimLookSpeed_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimLookSpeed(SightActor);
	}
	return AimLookSpeed;
}

float ABaseWeapon::GetAimLeaningScale_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimLeaningScale(SightActor);
	}
	return 1.0f;
}

float ABaseWeapon::GetAimBreathingScale_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimBreathingScale(SightActor);
	}
	return 0.3f;
}

bool ABaseWeapon::ShouldHideFPSMeshWhenAiming_Implementation() const
{
	AActor* SightActor = GetCurrentSightActor();
	if (SightActor && SightActor->Implements<USightInterface>())
	{
		return ISightInterface::Execute_ShouldHideFPSMeshWhenAiming(SightActor);
	}
	return false;
}

void ABaseWeapon::SetFPSMeshVisibility_Implementation(bool bVisible)
{
	if (FPSMesh)
	{
		FPSMesh->SetVisibility(bVisible, true);
	}
}

void ABaseWeapon::SetAiming_Implementation(bool bAiming)
{
	IsAiming = bAiming;
}

bool ABaseWeapon::GetIsAiming_Implementation() const
{
	return IsAiming;
}

bool ABaseWeapon::CanReload_Implementation() const
{
	if (!ReloadComponent) return false;
	return ReloadComponent->CanReload_Internal();
}

void ABaseWeapon::Reload_Implementation(const FUseContext& Ctx)
{
	if (!ReloadComponent) return;
	ReloadComponent->Server_StartReload(Ctx);
}

bool ABaseWeapon::IsReloading_Implementation() const
{
	if (!ReloadComponent) return false;
	return ReloadComponent->bIsReloading;
}

AActor* ABaseWeapon::GetMagazineActor_Implementation() const
{
	// Use replicated CurrentMagazine pointer for consistent access on all machines
	return CurrentMagazine;
}

UPrimitiveComponent* ABaseWeapon::GetFPSMagazineMesh_Implementation() const
{
	// Use replicated CurrentMagazine pointer
	if (CurrentMagazine && CurrentMagazine->Implements<UMagazineMeshProviderInterface>())
	{
		return IMagazineMeshProviderInterface::Execute_GetFPSMesh(CurrentMagazine);
	}
	return nullptr;
}

UPrimitiveComponent* ABaseWeapon::GetTPSMagazineMesh_Implementation() const
{
	// Use replicated CurrentMagazine pointer
	if (CurrentMagazine && CurrentMagazine->Implements<UMagazineMeshProviderInterface>())
	{
		return IMagazineMeshProviderInterface::Execute_GetTPSMesh(CurrentMagazine);
	}
	return nullptr;
}

UReloadComponent* ABaseWeapon::GetReloadComponent_Implementation() const
{
	return ReloadComponent;
}

void ABaseWeapon::OnWeaponReloadComplete_Implementation()
{
	// Base implementation does nothing
	// Override in child classes for weapon-specific behavior
	// Example: M4A1 resets bBoltCarrierOpen = false
}

TSubclassOf<UUserWidget> ABaseWeapon::GetItemWidgetClass_Implementation() const
{
	return ItemWidgetClass;
}
