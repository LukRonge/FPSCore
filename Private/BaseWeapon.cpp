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

DEFINE_LOG_CATEGORY_STATIC(LogFPSCore, Log, All);

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Network optimization
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(2.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	TPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPSMesh"));
	TPSMesh->SetupAttachment(SceneRoot);
	TPSMesh->SetOwnerNoSee(true);
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

	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FPSMesh->SetSimulatePhysics(false);
	// Performance optimizations
	FPSMesh->bComponentUseFixedSkelBounds = true;
	FPSMesh->SetGenerateOverlapEvents(false);

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
	UE_LOG(LogFPSCore, Warning, TEXT("[%s] UseStart_Implementation - Role=%s, Owner=%s, NetMode=%d"),
		*GetName(),
		*UEnum::GetValueAsString(GetLocalRole()),
		GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"),
		(int32)GetNetMode());
	Server_Shoot(true);
}

void ABaseWeapon::UseTick_Implementation(const FUseContext& Ctx)
{
}

void ABaseWeapon::UseStop_Implementation(const FUseContext& Ctx)
{
	UE_LOG(LogFPSCore, Warning, TEXT("[%s] UseStop_Implementation - Role=%s, Owner=%s, NetMode=%d"),
		*GetName(),
		*UEnum::GetValueAsString(GetLocalRole()),
		GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"),
		(int32)GetNetMode());
	Server_Shoot(false);
}

void ABaseWeapon::Server_Shoot_Implementation(bool bPressed)
{
	UE_LOG(LogFPSCore, Warning, TEXT("[%s] Server_Shoot_Implementation - Role=%s, bPressed=%d, FireComponent=%s, HasAuthority=%d"),
		*GetName(),
		*UEnum::GetValueAsString(GetLocalRole()),
		bPressed,
		FireComponent ? TEXT("Valid") : TEXT("NULL"),
		HasAuthority());

	if (!FireComponent)
	{
		UE_LOG(LogFPSCore, Error, TEXT("[%s] Server_Shoot - No FireComponent!"), *GetName());
		return;
	}

	if (bPressed)
	{
		UE_LOG(LogFPSCore, Warning, TEXT("[%s] Server_Shoot - Calling FireComponent->TriggerPulled()"), *GetName());
		FireComponent->TriggerPulled();
	}
	else
	{
		UE_LOG(LogFPSCore, Warning, TEXT("[%s] Server_Shoot - Calling FireComponent->TriggerReleased()"), *GetName());
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
	// Disable TPS mesh physics when picked up (will be attached to character)
	if (TPSMesh)
	{
		TPSMesh->SetSimulatePhysics(false);
		TPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABaseWeapon::OnDropped_Implementation(const FInteractionContext& Ctx)
{
	SetReplicateMovement(true);

	// Re-enable TPS mesh physics for world pickup
	if (TPSMesh)
	{
		TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TPSMesh->SetSimulatePhysics(true);
	}
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
		ENCPoolMethod::AutoRelease
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
			ENCPoolMethod::AutoRelease
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
	// Block during equip/unequip montage
	if (bIsEquipping || bIsUnequipping)
	{
		UE_LOG(LogFPSCore, Warning, TEXT("[%s] CanBeUnequipped - BLOCKED: bIsEquipping=%d, bIsUnequipping=%d"),
			*GetName(), bIsEquipping, bIsUnequipping);
		return false;
	}

	// Block during reload (delegate to component)
	if (ReloadComponent && ReloadComponent->bIsReloading)
	{
		UE_LOG(LogFPSCore, Warning, TEXT("[%s] CanBeUnequipped - BLOCKED: ReloadComponent->bIsReloading=true"),
			*GetName());
		return false;
	}

	// Block during weapon montage (shoot, inspect, etc.)
	if (FPSMesh)
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			if (AnimInst->IsAnyMontagePlaying())
			{
				UE_LOG(LogFPSCore, Warning, TEXT("[%s] CanBeUnequipped - BLOCKED: FPSMesh montage playing"),
					*GetName());
				return false;
			}
		}
	}

	return true;
}

// ============================================
// EQUIP/UNEQUIP MONTAGE INTERFACE
// ============================================

UAnimMontage* ABaseWeapon::GetEquipMontage_Implementation() const
{
	return EquipMontage;
}

UAnimMontage* ABaseWeapon::GetUnequipMontage_Implementation() const
{
	return UnequipMontage;
}

bool ABaseWeapon::IsEquipping_Implementation() const
{
	return bIsEquipping;
}

bool ABaseWeapon::IsUnequipping_Implementation() const
{
	return bIsUnequipping;
}

void ABaseWeapon::SetEquippingState_Implementation(bool bEquipping)
{
	bIsEquipping = bEquipping;
}

void ABaseWeapon::SetUnequippingState_Implementation(bool bUnequipping)
{
	bIsUnequipping = bUnequipping;
}

void ABaseWeapon::OnEquipMontageComplete_Implementation(APawn* OwningPawn)
{
	bIsEquipping = false;
	// Weapon is now ready to use
}

void ABaseWeapon::OnUnequipMontageComplete_Implementation(APawn* OwningPawn)
{
	bIsUnequipping = false;
	// Weapon is now holstered
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
	UE_LOG(LogFPSCore, Warning, TEXT("[%s] Reload_Implementation - Role=%s, ReloadComponent=%s, Owner=%s"),
		*GetName(),
		*UEnum::GetValueAsString(GetLocalRole()),
		ReloadComponent ? TEXT("Valid") : TEXT("NULL"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"));

	if (!ReloadComponent)
	{
		UE_LOG(LogFPSCore, Error, TEXT("[%s] Reload_Implementation - No ReloadComponent!"), *GetName());
		return;
	}

	UE_LOG(LogFPSCore, Warning, TEXT("[%s] Reload_Implementation - Calling ReloadComponent->Server_StartReload()"), *GetName());
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

// ============================================
// HELPER METHODS FOR CHILD CLASSES
// ============================================

void ABaseWeapon::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	// Use IHoldableInterface to access meshes (Golden Rule compliance)
	if (!Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(const_cast<ABaseWeapon*>(this));
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(const_cast<ABaseWeapon*>(this));

	// Play on FPS mesh (visible to owner)
	if (USkeletalMeshComponent* FPSSkeletalMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* FPSAnimInstance = FPSSkeletalMesh->GetAnimInstance())
		{
			FPSAnimInstance->Montage_Play(Montage);
		}
	}

	// Play on TPS mesh (visible to others)
	if (USkeletalMeshComponent* TPSSkeletalMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* TPSAnimInstance = TPSSkeletalMesh->GetAnimInstance())
		{
			TPSAnimInstance->Montage_Play(Montage);
		}
	}
}

void ABaseWeapon::ForceUpdateWeaponAnimInstances()
{
	// Use IHoldableInterface to access meshes (Golden Rule compliance)
	if (!Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(const_cast<ABaseWeapon*>(this));
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(const_cast<ABaseWeapon*>(this));

	// Force update FPS mesh AnimInstance
	if (USkeletalMeshComponent* FPSSkeletalMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* FPSAnimInstance = FPSSkeletalMesh->GetAnimInstance())
		{
			FPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}

	// Force update TPS mesh AnimInstance
	if (USkeletalMeshComponent* TPSSkeletalMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* TPSAnimInstance = TPSSkeletalMesh->GetAnimInstance())
		{
			TPSAnimInstance->NativeUpdateAnimation(0.0f);
		}
	}
}
