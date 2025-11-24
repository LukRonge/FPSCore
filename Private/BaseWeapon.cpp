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
	TPSMesh->SetIsReplicated(true);
	TPSMesh->bReplicatePhysicsToAutonomousProxy = true;

	FPSMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPSMesh"));
	FPSMesh->SetupAttachment(SceneRoot);
	FPSMesh->SetOnlyOwnerSee(true);
	FPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BallisticsComponent = CreateDefaultSubobject<UBallisticsComponent>(TEXT("BallisticsComponent"));
	FireComponent = nullptr;

	FPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("FPSMagazineComponent"));
	FPSMagazineComponent->SetupAttachment(FPSMesh, FName("magazine"));

	TPSMagazineComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("TPSMagazineComponent"));
	TPSMagazineComponent->SetupAttachment(TPSMesh, FName("magazine"));

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

	if (MagazineClass)
	{
		FPSMagazineComponent->SetChildActorClass(MagazineClass);
		TPSMagazineComponent->SetChildActorClass(MagazineClass);
	}

	if (DefaultSightClass)
	{
		CurrentSightClass = DefaultSightClass;
		InitSightComponents(CurrentSightClass);
	}

	if (!ReloadComponent)
	{
		ReloadComponent = FindComponentByClass<UReloadComponent>();
	}
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (!FireComponent)
	{
		FireComponent = FindComponentByClass<UFireComponent>();
	}

	if (!ReloadComponent)
	{
		ReloadComponent = FindComponentByClass<UReloadComponent>();
	}

	if (HasAuthority() && TPSMagazineComponent->GetChildActor())
	{
		// Use direct cast here - CurrentMagazine is authoritative replicated reference
		// This is internal initialization, not external access pattern
		CurrentMagazine = Cast<ABaseMagazine>(TPSMagazineComponent->GetChildActor());

		if (FireComponent && BallisticsComponent)
		{
			FireComponent->BallisticsComponent = BallisticsComponent;

			if (CurrentMagazine)
			{
				BallisticsComponent->InitAmmoType(CurrentMagazine->AmmoType);
			}
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseWeapon, CurrentMagazine);
	DOREPLIFETIME(ABaseWeapon, CurrentSightClass);
}

void ABaseWeapon::OnRep_CurrentMagazine()
{
}

void ABaseWeapon::SetOwner(AActor* NewOwner)
{
	Super::SetOwner(NewOwner);

	if (FPSMagazineComponent->GetChildActor())
	{
		FPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
	}

	if (TPSMagazineComponent->GetChildActor())
	{
		TPSMagazineComponent->GetChildActor()->SetOwner(NewOwner);
	}

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
		Multicast_PlayMuzzleFlash(MuzzleLocation, Direction);
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

void ABaseWeapon::Multicast_PlayMuzzleFlash_Implementation(
	FVector_NetQuantize MuzzleLocation,
	FVector_NetQuantizeNormal Direction)
{
	if (MuzzleFlashNiagara)
	{
		USkeletalMeshComponent* MuzzleMesh = nullptr;

		// Check if owner is locally controlled pawn to select FPS or TPS mesh
		AActor* OwnerActor = GetOwner();
		APawn* OwnerPawn = OwnerActor ? Cast<APawn>(OwnerActor) : nullptr;
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			MuzzleMesh = FPSMesh;
		}
		else
		{
			MuzzleMesh = TPSMesh;
		}

		if (MuzzleMesh)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				MuzzleFlashNiagara,
				MuzzleMesh,
				FName("barrel"),
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true,
				true,
				ENCPoolMethod::None
			);
		}
	}

	if (ShootMontage)
	{
		AActor* WeaponOwner = GetOwner();
		if (WeaponOwner && WeaponOwner->Implements<UCharacterMeshProviderInterface>())
		{
			USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(WeaponOwner);
			if (BodyMesh)
			{
				UAnimInstance* BodyAnimInst = BodyMesh->GetAnimInstance();
				if (BodyAnimInst)
				{
					BodyAnimInst->Montage_Play(ShootMontage);
				}
			}

			USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(WeaponOwner);
			if (ArmsMesh)
			{
				UAnimInstance* ArmsAnimInst = ArmsMesh->GetAnimInstance();
				if (ArmsAnimInst)
				{
					ArmsAnimInst->Montage_Play(ShootMontage);
				}
			}

			USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(WeaponOwner);
			if (LegsMesh)
			{
				UAnimInstance* LegsAnimInst = LegsMesh->GetAnimInstance();
				if (LegsAnimInst)
				{
					LegsAnimInst->Montage_Play(ShootMontage);
				}
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
	return 0;
}

int32 ABaseWeapon::ConsumeAmmo_Implementation(int32 Requested, const FUseContext& Ctx)
{
	if (!HasAuthority()) return 0;
	if (!CurrentMagazine || CurrentMagazine->CurrentAmmo <= 0) return 0;
	if (ReloadComponent && ReloadComponent->bIsReloading) return 0;

	int32 AmmoToConsume = FMath::Min(Requested, CurrentMagazine->CurrentAmmo);

	for (int32 i = 0; i < AmmoToConsume; i++)
	{
		CurrentMagazine->RemoveAmmo();
	}

	return AmmoToConsume;
}

void ABaseWeapon::InitSightComponents(TSubclassOf<ABaseSight> SightClass)
{
	if (!FPSSightComponent || !TPSSightComponent) return;

	if (SightClass)
	{
		FPSSightComponent->SetChildActorClass(TSubclassOf<AActor>(SightClass));
		TPSSightComponent->SetChildActorClass(TSubclassOf<AActor>(SightClass));

		FPSSightComponent->CreateChildActor();
		if (ABaseSight* FPSSight = Cast<ABaseSight>(FPSSightComponent->GetChildActor()))
		{
			FPSSight->SetReplicates(false);
			FPSSight->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			FPSSight->SetOwner(GetOwner());
		}

		TPSSightComponent->CreateChildActor();
		if (ABaseSight* TPSSight = Cast<ABaseSight>(TPSSightComponent->GetChildActor()))
		{
			TPSSight->SetReplicates(false);
			TPSSight->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
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

AActor* ABaseWeapon::GetFPSMagazineActor_Implementation() const
{
	if (FPSMagazineComponent)
	{
		return FPSMagazineComponent->GetChildActor();
	}
	return nullptr;
}

AActor* ABaseWeapon::GetTPSMagazineActor_Implementation() const
{
	if (TPSMagazineComponent)
	{
		return TPSMagazineComponent->GetChildActor();
	}
	return nullptr;
}

UReloadComponent* ABaseWeapon::GetReloadComponent_Implementation() const
{
	return ReloadComponent;
}

TSubclassOf<UUserWidget> ABaseWeapon::GetItemWidgetClass_Implementation() const
{
	return ItemWidgetClass;
}

void ABaseWeapon::SyncVisualMagazines()
{
	if (!CurrentMagazine) return;

	int32 AuthoritativeAmmo = CurrentMagazine->CurrentAmmo;

	// Sync FPS magazine via interface
	AActor* FPSMagActor = FPSMagazineComponent ? FPSMagazineComponent->GetChildActor() : nullptr;
	if (FPSMagActor && FPSMagActor->Implements<UAmmoProviderInterface>())
	{
		IAmmoProviderInterface::Execute_SetCurrentAmmo(FPSMagActor, AuthoritativeAmmo);
	}

	// Sync TPS magazine via interface
	AActor* TPSMagActor = TPSMagazineComponent ? TPSMagazineComponent->GetChildActor() : nullptr;
	if (TPSMagActor && TPSMagActor->Implements<UAmmoProviderInterface>())
	{
		IAmmoProviderInterface::Execute_SetCurrentAmmo(TPSMagActor, AuthoritativeAmmo);
	}
}
