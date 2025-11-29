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
	// NOTE: Do NOT disable tick - required for animations (montages) to play
	// FPSMesh->PrimaryComponentTick.bCanEverTick = false;

	// NOTE: BallisticsComponent is NOT created here - child classes create their own
	// (e.g., SPAS12 creates UShotgunBallisticsComponent, others use UBallisticsComponent)
	BallisticsComponent = nullptr;
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

	// Find components that may be added via Blueprint
	if (!FireComponent)
	{
		FireComponent = FindComponentByClass<UFireComponent>();
	}

	if (!ReloadComponent)
	{
		ReloadComponent = FindComponentByClass<UReloadComponent>();
	}

	// SERVER ONLY: Initialize magazine and sight components
	// Clients receive actor pointers via OnRep_CurrentMagazine / OnRep_CurrentSight
	if (HasAuthority())
	{
		InitMagazineComponents(DefaultMagazineClass);
		InitSightComponents(DefaultSightClass);
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	// SERVER ONLY: Link FireComponent to BallisticsComponent
	if (HasAuthority() && FireComponent && BallisticsComponent)
	{
		FireComponent->BallisticsComponent = BallisticsComponent;

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
	if (CurrentMagazine)
	{
		if (BallisticsComponent)
		{
			BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
		}

		CurrentMagazine->SetOwner(GetOwner());
		AttachMagazineMeshes();
	}
}

void ABaseWeapon::SetOwner(AActor* NewOwner)
{
	Super::SetOwner(NewOwner);
	PropagateOwnerToChildActors(NewOwner);
}

void ABaseWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	PropagateOwnerToChildActors(GetOwner());
}

void ABaseWeapon::PropagateOwnerToChildActors(AActor* NewOwner)
{
	if (CurrentMagazine)
	{
		CurrentMagazine->SetOwner(NewOwner);
	}

	if (CurrentSight)
	{
		CurrentSight->SetOwner(NewOwner);
	}
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
	// Base implementation - override in child classes for weapon-specific behavior
}

void ABaseWeapon::OnUnequipped_Implementation()
{
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

FName ABaseWeapon::GetReloadAttachSocket_Implementation() const
{
	return ReloadAttachSocket;
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
	// SERVER ONLY - clients receive CurrentSight via replication
	if (!HasAuthority() || !SightComponent)
	{
		return;
	}

	if (SightClass)
	{
		SightComponent->SetChildActorClass(TSubclassOf<AActor>(SightClass));

		AActor* SightActor = SightComponent->GetChildActor();
		if (SightActor)
		{
			CurrentSight = Cast<ABaseSight>(SightActor);
			SightActor->SetOwner(GetOwner());

			// Attach SightComponent to FPSMesh at the socket specified by sight
			if (CurrentSight->Implements<USightMeshProviderInterface>())
			{
				FName AttachSocket = ISightMeshProviderInterface::Execute_GetAttachSocket(CurrentSight);
				SightComponent->AttachToComponent(FPSMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
			}

			AttachSightMeshes();
		}
	}
	else
	{
		SightComponent->SetChildActorClass(nullptr);
		CurrentSight = nullptr;
	}
}

void ABaseWeapon::AttachSightMeshes()
{
	if (!CurrentSight || !CurrentSight->Implements<USightMeshProviderInterface>())
	{
		return;
	}

	UPrimitiveComponent* FPSSightMesh = ISightMeshProviderInterface::Execute_GetFPSMesh(CurrentSight);
	UPrimitiveComponent* TPSSightMesh = ISightMeshProviderInterface::Execute_GetTPSMesh(CurrentSight);
	FName AttachSocket = ISightMeshProviderInterface::Execute_GetAttachSocket(CurrentSight);

	if (FPSSightMesh && FPSMesh)
	{
		FPSSightMesh->AttachToComponent(FPSMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	}

	if (TPSSightMesh && TPSMesh)
	{
		TPSSightMesh->AttachToComponent(TPSMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	}
}

void ABaseWeapon::OnRep_CurrentSight()
{
	if (CurrentSight)
	{
		CurrentSight->SetOwner(GetOwner());

		// Attach SightComponent to FPSMesh at the socket specified by sight
		if (SightComponent && CurrentSight->Implements<USightMeshProviderInterface>())
		{
			FName AttachSocket = ISightMeshProviderInterface::Execute_GetAttachSocket(CurrentSight);
			SightComponent->AttachToComponent(FPSMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
		}

		AttachSightMeshes();
	}
}

void ABaseWeapon::InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass)
{
	// SERVER ONLY - clients receive CurrentMagazine via replication
	if (!HasAuthority() || !MagazineComponent)
	{
		return;
	}

	if (MagazineClass)
	{
		MagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));

		AActor* MagActor = MagazineComponent->GetChildActor();
		if (MagActor)
		{
			CurrentMagazine = Cast<ABaseMagazine>(MagActor);
			MagActor->SetOwner(GetOwner());
			AttachMagazineMeshes();
		}
	}
	else
	{
		MagazineComponent->SetChildActorClass(nullptr);
		CurrentMagazine = nullptr;
	}
}

void ABaseWeapon::AttachMagazineMeshes()
{
	if (!CurrentMagazine || !CurrentMagazine->Implements<UMagazineMeshProviderInterface>())
	{
		return;
	}

	UPrimitiveComponent* FPSMagMesh = IMagazineMeshProviderInterface::Execute_GetFPSMesh(CurrentMagazine);
	UPrimitiveComponent* TPSMagMesh = IMagazineMeshProviderInterface::Execute_GetTPSMesh(CurrentMagazine);

	if (FPSMagMesh && FPSMesh)
	{
		FPSMagMesh->AttachToComponent(FPSMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("magazine"));
	}

	if (TPSMagMesh && TPSMesh)
	{
		TPSMagMesh->AttachToComponent(TPSMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("magazine"));
	}
}

TSubclassOf<UUserWidget> ABaseWeapon::GetCrossHair_Implementation() const
{
	return CrossHair;
}

TSubclassOf<UUserWidget> ABaseWeapon::GetAimingCrosshair_Implementation() const
{
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimingCrosshair(CurrentSight);
	}
	return nullptr;
}

FTransform ABaseWeapon::GetAimTransform_Implementation() const
{
	// If sight is attached, delegate to sight's aim transform
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimTransform(CurrentSight);
	}

	// No sight attached - use FPSMesh "aim" socket as aiming point
	// The "aim" socket represents the default iron sight aiming position
	if (FPSMesh && FPSMesh->DoesSocketExist(FName("aim")))
	{
		// Return world-space transform of the "aim" socket
		return FPSMesh->GetSocketTransform(FName("aim"), ERelativeTransformSpace::RTS_World);
	}

	// No sight and no "aim" socket - aiming not possible
	// Return invalid transform (scale = 0) as marker
	return FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
}

float ABaseWeapon::GetAimingFOV_Implementation() const
{
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		float SightFOV = ISightInterface::Execute_GetAimingFOV(CurrentSight);
		if (SightFOV > 0.0f)
		{
			return SightFOV;
		}
	}
	return AimFOV;
}

float ABaseWeapon::GetAimLookSpeed_Implementation() const
{
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimLookSpeed(CurrentSight);
	}
	return AimLookSpeed;
}

float ABaseWeapon::GetAimLeaningScale_Implementation() const
{
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimLeaningScale(CurrentSight);
	}
	return 1.0f;
}

float ABaseWeapon::GetAimBreathingScale_Implementation() const
{
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		return ISightInterface::Execute_GetAimBreathingScale(CurrentSight);
	}
	return 0.3f;
}

bool ABaseWeapon::ShouldHideFPSMeshWhenAiming_Implementation() const
{
	if (CurrentSight && CurrentSight->Implements<USightInterface>())
	{
		return ISightInterface::Execute_ShouldHideFPSMeshWhenAiming(CurrentSight);
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

bool ABaseWeapon::CanBeUnequipped_Implementation() const
{
	// Block during reload (delegate to component)
	if (ReloadComponent && ReloadComponent->bIsReloading)
	{
		return false;
	}

	// Block during weapon montage (shoot, inspect, etc.)
	if (FPSMesh)
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			if (AnimInst->IsAnyMontagePlaying())
			{
				return false;
			}
		}
	}

	return true;
}

bool ABaseWeapon::CanAim_Implementation() const
{
	// Block aiming during reload
	if (ReloadComponent && ReloadComponent->bIsReloading)
	{
		return false;
	}

	return true;
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
