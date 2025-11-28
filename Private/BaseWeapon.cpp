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
	MagazineComponent->SetIsReplicated(false);  // Child actor spawned via OnRep_CurrentMagazineClass

	FPSSightComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("FPSSightComponent"));
	FPSSightComponent->SetupAttachment(FPSMesh, FName("attachment0"));
	FPSSightComponent->SetIsReplicated(false);

	TPSSightComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("TPSSightComponent"));
	TPSSightComponent->SetupAttachment(TPSMesh, FName("attachment0"));
	TPSSightComponent->SetIsReplicated(false);
}

void ABaseWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

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
	// Remote clients receive these via OnRep_CurrentMagazineClass / OnRep_CurrentSightClass
	// This prevents ChildActor destruction caused by bReplicates=true conflict

	if (HasAuthority())
	{
		if (DefaultMagazineClass)
		{
			CurrentMagazineClass = DefaultMagazineClass;
		}

		if (DefaultSightClass)
		{
			CurrentSightClass = DefaultSightClass;
		}

		InitMagazineComponents(DefaultMagazineClass);
		InitSightComponents(DefaultSightClass);
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	// ============================================
	// REMOTE CLIENT: Initialize magazine/sight child actors from replicated class
	// ============================================
	// On remote clients, CurrentMagazineClass/CurrentSightClass are replicated BEFORE BeginPlay.
	// Child actors must be created here because ChildActorComponent::SetChildActorClass()
	// requires the component to be fully registered (which happens after BeginPlay starts).
	// OnRep callbacks may fire before component registration is complete.
	if (!HasAuthority())
	{
		if (CurrentMagazineClass)
		{
			InitMagazineComponents(CurrentMagazineClass);
		}
		if (CurrentSightClass)
		{
			InitSightComponents(CurrentSightClass);
		}
	}

	// ============================================
	// SERVER ONLY: Link FireComponent to BallisticsComponent
	// ============================================
	if (HasAuthority() && FireComponent && BallisticsComponent)
	{
		FireComponent->BallisticsComponent = BallisticsComponent;

		// Initialize ballistics with magazine's ammo type
		// CurrentMagazine is set in InitMagazineComponents via SetChildActorClass
		if (CurrentMagazine)
		{
			BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseWeapon, CurrentMagazine);
	DOREPLIFETIME(ABaseWeapon, CurrentMagazineClass);
	DOREPLIFETIME(ABaseWeapon, CurrentSightClass);
}

void ABaseWeapon::OnRep_CurrentMagazine()
{
	// CLIENT ONLY - called when CurrentMagazine reference is replicated from server
	// Synchronize BallisticsComponent with new magazine's ammo type
	if (BallisticsComponent && CurrentMagazine)
	{
		BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
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
	// Propagate owner to magazine component
	if (MagazineComponent && MagazineComponent->GetChildActor())
	{
		MagazineComponent->GetChildActor()->SetOwner(NewOwner);
	}

	// Propagate owner to sight components
	if (FPSSightComponent && FPSSightComponent->GetChildActor())
	{
		FPSSightComponent->GetChildActor()->SetOwner(NewOwner);
	}

	if (TPSSightComponent && TPSSightComponent->GetChildActor())
	{
		TPSSightComponent->GetChildActor()->SetOwner(NewOwner);
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
		// TPSMesh effects for OTHER players (Multicast runs on all clients)
		Multicast_PlayMuzzleFlash();

		// FPSMesh effects for OWNING CLIENT only (Client RPC runs on owner)
		Client_PlayMuzzleFlash();
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

void ABaseWeapon::SpawnMuzzleFlashOnMesh(USkeletalMeshComponent* Mesh)
{
	if (!MuzzleFlashNiagara || !Mesh)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAttached(
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
}

void ABaseWeapon::Multicast_PlayMuzzleFlash_Implementation()
{
	// TPSMesh muzzle flash (visible to others due to OwnerNoSee)
	SpawnMuzzleFlashOnMesh(TPSMesh);

	// Character shoot montage on Body + Legs mesh
	if (ShootMontage)
	{
		AActor* WeaponOwner = GetOwner();
		if (WeaponOwner && WeaponOwner->Implements<UCharacterMeshProviderInterface>())
		{
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
	}
}

void ABaseWeapon::Client_PlayMuzzleFlash_Implementation()
{
	// Safety check - Client RPC should only run on locally controlled client
	AActor* WeaponOwner = GetOwner();
	APawn* OwnerPawn = WeaponOwner ? Cast<APawn>(WeaponOwner) : nullptr;
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	// FPSMesh muzzle flash (visible to owner due to OnlyOwnerSee)
	SpawnMuzzleFlashOnMesh(FPSMesh);

	// Character shoot montage on Arms mesh (FPS view)
	if (ShootMontage && WeaponOwner->Implements<UCharacterMeshProviderInterface>())
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
	if (!FPSSightComponent || !TPSSightComponent) return;

	if (SightClass)
	{
		FPSSightComponent->SetChildActorClass(TSubclassOf<AActor>(SightClass));
		TPSSightComponent->SetChildActorClass(TSubclassOf<AActor>(SightClass));

		// FPS Sight: FirstPerson visibility (only owner sees)
		if (ABaseSight* FPSSight = Cast<ABaseSight>(FPSSightComponent->GetChildActor()))
		{
			FPSSight->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			FPSSight->ApplyVisibilityToMeshes();
			FPSSight->SetOwner(GetOwner());
		}

		// TPS Sight: WorldSpaceRepresentation visibility (others see, owner doesn't)
		if (ABaseSight* TPSSight = Cast<ABaseSight>(TPSSightComponent->GetChildActor()))
		{
			TPSSight->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
			TPSSight->ApplyVisibilityToMeshes();
			TPSSight->SetOwner(GetOwner());
		}
	}
	else
	{
		FPSSightComponent->SetChildActorClass(nullptr);
		TPSSightComponent->SetChildActorClass(nullptr);
	}
}

void ABaseWeapon::OnRep_CurrentSightClass()
{
	InitSightComponents(CurrentSightClass);
}

void ABaseWeapon::InitMagazineComponents(TSubclassOf<ABaseMagazine> MagazineClass)
{
	if (!MagazineComponent) return;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::InitMagazineComponents() - %s, MagazineClass=%s, HasAuthority=%d"),
		*GetName(),
		MagazineClass ? *MagazineClass->GetName() : TEXT("nullptr"),
		HasAuthority());
#endif

	if (MagazineClass)
	{
		MagazineComponent->SetChildActorClass(TSubclassOf<AActor>(MagazineClass));

		AActor* MagActor = MagazineComponent->GetChildActor();
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, Verbose, TEXT("  -> After SetChildActorClass: ChildActor=%s"),
			MagActor ? *MagActor->GetName() : TEXT("nullptr"));
#endif

		if (MagActor)
		{
			// SERVER ONLY: Set authoritative magazine reference
			// CurrentMagazine is ABaseMagazine* UPROPERTY required for replication
			// MagazineClass guarantees the child actor is ABaseMagazine subclass
			if (HasAuthority())
			{
				CurrentMagazine = Cast<ABaseMagazine>(MagActor);
			}

			// Propagate owner for correct visibility
			MagActor->SetOwner(GetOwner());

			// Attach magazine meshes to weapon meshes
			// This is a LOCAL operation - each machine does it independently
			AttachMagazineMeshes();
		}
	}
	else
	{
		MagazineComponent->SetChildActorClass(nullptr);
		if (HasAuthority())
		{
			CurrentMagazine = nullptr;
		}
	}
}

void ABaseWeapon::AttachMagazineMeshes()
{
	AActor* MagActor = MagazineComponent ? MagazineComponent->GetChildActor() : nullptr;
	if (!MagActor) return;

	if (!MagActor->Implements<UMagazineMeshProviderInterface>()) return;

	UPrimitiveComponent* FPSMagMesh = IMagazineMeshProviderInterface::Execute_GetFPSMesh(MagActor);
	UPrimitiveComponent* TPSMagMesh = IMagazineMeshProviderInterface::Execute_GetTPSMesh(MagActor);

	// FPS magazine mesh -> FPS weapon mesh (visible only to owner)
	if (FPSMagMesh && FPSMesh)
	{
		FPSMagMesh->AttachToComponent(
			FPSMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			FName("magazine")
		);
	}

	// TPS magazine mesh -> TPS weapon mesh (visible to others)
	if (TPSMagMesh && TPSMesh)
	{
		TPSMagMesh->AttachToComponent(
			TPSMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			FName("magazine")
		);
	}
}

void ABaseWeapon::OnRep_CurrentMagazineClass()
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	UE_LOG(LogTemp, Verbose, TEXT("BaseWeapon::OnRep_CurrentMagazineClass() - %s, CurrentMagazineClass=%s, HasBegunPlay=%d"),
		*GetName(),
		CurrentMagazineClass ? *CurrentMagazineClass->GetName() : TEXT("nullptr"),
		HasActorBegunPlay());
#endif

	// Skip if called before BeginPlay - BeginPlay will handle initialization
	// OnRep can fire before component registration is complete, causing SetChildActorClass to fail
	if (!HasActorBegunPlay())
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		UE_LOG(LogTemp, Verbose, TEXT("  -> Skipping: BeginPlay not called yet"));
#endif
		return;
	}

	InitMagazineComponents(CurrentMagazineClass);
}

TSubclassOf<UUserWidget> ABaseWeapon::GetCrossHair_Implementation() const
{
	return CrossHair;
}

AActor* ABaseWeapon::GetCurrentSightActor() const
{
	if (FPSSightComponent)
	{
		return FPSSightComponent->GetChildActor();
	}
	return nullptr;
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
	if (MagazineComponent)
	{
		return MagazineComponent->GetChildActor();
	}
	return nullptr;
}

UPrimitiveComponent* ABaseWeapon::GetFPSMagazineMesh_Implementation() const
{
	AActor* MagActor = MagazineComponent ? MagazineComponent->GetChildActor() : nullptr;
	if (MagActor && MagActor->Implements<UMagazineMeshProviderInterface>())
	{
		return IMagazineMeshProviderInterface::Execute_GetFPSMesh(MagActor);
	}
	return nullptr;
}

UPrimitiveComponent* ABaseWeapon::GetTPSMagazineMesh_Implementation() const
{
	AActor* MagActor = MagazineComponent ? MagazineComponent->GetChildActor() : nullptr;
	if (MagActor && MagActor->Implements<UMagazineMeshProviderInterface>())
	{
		return IMagazineMeshProviderInterface::Execute_GetTPSMesh(MagActor);
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
