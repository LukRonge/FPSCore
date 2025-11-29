// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/PumpActionFireComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/ReloadableInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogPumpActionFire, Log, All);

UPumpActionFireComponent::UPumpActionFireComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Pump-action defaults (SPAS-12 semi-auto mode)
	FireRate = 220.0f;  // ~220 RPM theoretical max (pump-action limited)
	SpreadScale = 1.5f;  // Wider spread (shotgun buckshot)
	RecoilScale = 2.0f;  // Higher recoil (12 gauge)
}

void UPumpActionFireComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPumpActionFireComponent, bIsPumping);
	DOREPLIFETIME(UPumpActionFireComponent, bChamberEmpty);
}

// ============================================
// ONREP CALLBACKS
// ============================================

void UPumpActionFireComponent::OnRep_IsPumping()
{
	UE_LOG(LogPumpActionFire, Log, TEXT("[Client] OnRep_IsPumping: %s"), bIsPumping ? TEXT("true") : TEXT("false"));

	PropagateStateToAnimInstances();

	// If pumping started on client, reattach weapon and play montages
	if (bIsPumping)
	{
		ReattachWeaponToSocket(true);  // To reload socket (weapon_l)
		PlayPumpActionMontages();
	}
	else
	{
		// Pump-action ended - reattach weapon back to normal socket
		ReattachWeaponToSocket(false);  // To equip socket (weapon_r)
	}
}

void UPumpActionFireComponent::OnRep_ChamberEmpty()
{
	PropagateStateToAnimInstances();
}

void UPumpActionFireComponent::PropagateStateToAnimInstances()
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>()) return;

	// Get weapon meshes and update their AnimInstances
	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	// Internal cast: UPrimitiveComponent* from interface to USkeletalMeshComponent* for AnimInstance access
	// This is required because IHoldableInterface returns UPrimitiveComponent* for flexibility
	if (USkeletalMeshComponent* FPSMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			AnimInst->NativeUpdateAnimation(0.0f);
		}
	}

	// Internal cast: UPrimitiveComponent* from interface to USkeletalMeshComponent* for AnimInstance access
	if (USkeletalMeshComponent* TPSMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* AnimInst = TPSMesh->GetAnimInstance())
		{
			AnimInst->NativeUpdateAnimation(0.0f);
		}
	}
}

// ============================================
// FIRE CONTROL OVERRIDE
// ============================================

bool UPumpActionFireComponent::CanFire() const
{
	// Pump-action specific checks
	if (bIsPumping)
	{
		UE_LOG(LogPumpActionFire, Verbose, TEXT("CanFire: false - bIsPumping is true"));
		return false;
	}

	if (bChamberEmpty)
	{
		UE_LOG(LogPumpActionFire, Verbose, TEXT("CanFire: false - bChamberEmpty is true"));
		return false;
	}

	// Base class checks (ammo, BallisticsComponent, etc.)
	bool bCanFire = Super::CanFire();
	UE_LOG(LogPumpActionFire, Verbose, TEXT("CanFire: %s (base check)"), bCanFire ? TEXT("true") : TEXT("false"));
	return bCanFire;
}

void UPumpActionFireComponent::TriggerPulled()
{
	UE_LOG(LogPumpActionFire, Log, TEXT("TriggerPulled - CanFire: %s, bTriggerHeld: %s, Authority: %s"),
		CanFire() ? TEXT("true") : TEXT("false"),
		bTriggerHeld ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	if (CanFire() && !bTriggerHeld)
	{
		bTriggerHeld = true;
		Fire();

		// NOTE: Pump-action sequence is NOT started here immediately!
		// It will be started after ShootMontage ends via OnShootMontageEnded callback
		// The weapon class (Spas12) registers the delegate in Multicast_PlayShootEffects
		// This ensures proper animation sequencing: shoot recoil -> pump manipulation

		// Mark that pump-action is pending (SERVER ONLY)
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			bPumpActionPendingAfterShoot = true;
			UE_LOG(LogPumpActionFire, Log, TEXT("[Server] Fire complete - pump-action pending after shoot montage"));
		}
	}
}

void UPumpActionFireComponent::TriggerReleased()
{
	bTriggerHeld = false;
	bWantsToFireAfterPump = false;
}

// ============================================
// PUMP-ACTION API
// ============================================

void UPumpActionFireComponent::StartPumpAction(bool bFromReload)
{
	if (!GetOwner()) return;

	UE_LOG(LogPumpActionFire, Log, TEXT("StartPumpAction - bFromReload: %s, Authority: %s"),
		bFromReload ? TEXT("true") : TEXT("false"),
		GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// SERVER: Set state
	if (GetOwner()->HasAuthority())
	{
		bIsPumping = true;

		// If from reload, chamber is being loaded
		if (bFromReload)
		{
			bChamberEmpty = false;
		}

		PropagateStateToAnimInstances();

		// Reattach weapon to reload socket (weapon_l) for pump manipulation
		ReattachWeaponToSocket(true);
	}

	// Play montages (LOCAL - runs on server immediately, clients via OnRep)
	PlayPumpActionMontages();
}

void UPumpActionFireComponent::OnPumpActionComplete()
{
	UE_LOG(LogPumpActionFire, Log, TEXT("OnPumpActionComplete - Authority: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// SERVER: Update state
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bIsPumping = false;

		// Check if magazine is empty after chambering
		AActor* WeaponActor = GetOwner();
		if (WeaponActor && WeaponActor->Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(WeaponActor);
			if (CurrentAmmo <= 0)
			{
				bChamberEmpty = true;
				UE_LOG(LogPumpActionFire, Log, TEXT("[Server] Chamber empty - no ammo to chamber"));
			}
		}

		PropagateStateToAnimInstances();

		// Reattach weapon back to equip socket (weapon_r)
		ReattachWeaponToSocket(false);

		UE_LOG(LogPumpActionFire, Log, TEXT("[Server] Pump-action complete - bIsPumping: false, bChamberEmpty: %s"),
			bChamberEmpty ? TEXT("true") : TEXT("false"));

		// Full-auto mode: Fire again if trigger still held
		if (bFullAutoTrigger && bTriggerHeld && CanFire())
		{
			UE_LOG(LogPumpActionFire, Log, TEXT("[Server] Full-auto mode - trigger held, firing again"));
			Fire();
			bPumpActionPendingAfterShoot = true;
		}
	}
}

void UPumpActionFireComponent::OnPumpActionStart()
{
	// LOCAL operation - start weapon pump-action montage
	// Called by AnimNotify_PumpActionStart when character hand grabs pump handle
	// This synchronizes weapon pump animation with character hand movement

	PlayWeaponMontage(WeaponPumpActionMontage);
}

void UPumpActionFireComponent::OnShellEject()
{
	// LOCAL operation - spawn shell casing VFX
	// TODO: Spawn shell casing particle/mesh at ejection port socket
	// This is purely visual, runs on all machines independently

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor) return;

	// Shell ejection VFX would be spawned here
	// Using BallisticsComponent's ammo data asset for shell mesh
	if (BallisticsComponent)
	{
		// BallisticsComponent->SpawnShellCasing() could be called here
		// For now, this is a placeholder for the VFX system
	}
}

void UPumpActionFireComponent::OnChamberRound()
{
	// LOCAL visual feedback + SERVER state check
	// The actual ammo consumption happened during Fire()
	// This notify is for audio/visual feedback of pump closing

	// SERVER: Check if we have ammo to chamber
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		AActor* WeaponActor = GetOwner();
		if (WeaponActor && WeaponActor->Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(WeaponActor);
			if (CurrentAmmo <= 0)
			{
				// No ammo to chamber - will be set in OnPumpActionComplete
				// This prevents the "click" of chambering when empty
			}
		}
	}
}

void UPumpActionFireComponent::ResetChamberState()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bChamberEmpty = false;
		PropagateStateToAnimInstances();
	}
}

void UPumpActionFireComponent::OnShootMontageEnded()
{
	UE_LOG(LogPumpActionFire, Log, TEXT("OnShootMontageEnded - bPumpActionPendingAfterShoot: %s, Authority: %s"),
		bPumpActionPendingAfterShoot ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// SERVER ONLY: Start pump-action if pending
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bPumpActionPendingAfterShoot)
	{
		bPumpActionPendingAfterShoot = false;
		UE_LOG(LogPumpActionFire, Log, TEXT("[Server] Shoot montage ended - starting pump-action sequence"));
		StartPumpAction(false);
	}
}

// ============================================
// INTERNAL HELPERS
// ============================================

void UPumpActionFireComponent::PlayPumpActionMontages()
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor) return;

	AActor* CharacterActor = WeaponActor->GetOwner();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	// Play on character meshes (Body, Arms, Legs)
	// NOTE: Weapon montage is NOT started here - it's triggered by AnimNotify_PumpActionStart
	// when character's hand reaches the pump handle for perfect synchronization
	if (PumpActionMontage)
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
		USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

		if (BodyMesh)
		{
			if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(PumpActionMontage);

				// Bind end delegate only on Body mesh (single callback)
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UPumpActionFireComponent::OnMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, PumpActionMontage);
			}
		}

		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(PumpActionMontage);
			}
		}

		if (LegsMesh)
		{
			if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(PumpActionMontage);
			}
		}
	}

	// NOTE: Weapon pump-action montage is triggered by AnimNotify_PumpActionStart
	// in character montage for synchronized hand/pump animation
}

void UPumpActionFireComponent::StopPumpActionMontages()
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor) return;

	AActor* CharacterActor = WeaponActor->GetOwner();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	if (PumpActionMontage)
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
		USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

		if (BodyMesh)
		{
			if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(PumpActionMontage))
				{
					AnimInst->Montage_Stop(0.2f, PumpActionMontage);
				}
			}
		}

		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(PumpActionMontage))
				{
					AnimInst->Montage_Stop(0.2f, PumpActionMontage);
				}
			}
		}

		if (LegsMesh)
		{
			if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(PumpActionMontage))
				{
					AnimInst->Montage_Stop(0.2f, PumpActionMontage);
				}
			}
		}
	}

	// Also stop weapon pump-action montage
	StopWeaponMontage(WeaponPumpActionMontage);
}

void UPumpActionFireComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != PumpActionMontage) return;

	// SERVER ONLY: Handle state changes and weapon reattachment
	// Clients receive state via OnRep_IsPumping which handles reattachment
	// This prevents race condition where client reattaches before OnRep arrives
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		OnPumpActionComplete();
	}
}

void UPumpActionFireComponent::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	// Internal cast: UPrimitiveComponent* from interface to USkeletalMeshComponent* for AnimInstance access
	if (USkeletalMeshComponent* FPSMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(Montage);
		}
	}

	// Internal cast: UPrimitiveComponent* from interface to USkeletalMeshComponent* for AnimInstance access
	if (USkeletalMeshComponent* TPSMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* AnimInst = TPSMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(Montage);
		}
	}
}

void UPumpActionFireComponent::StopWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	// Internal cast: UPrimitiveComponent* from interface to USkeletalMeshComponent* for AnimInstance access
	if (USkeletalMeshComponent* FPSMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			if (AnimInst->Montage_IsPlaying(Montage))
			{
				AnimInst->Montage_Stop(0.2f, Montage);
			}
		}
	}

	// Internal cast: UPrimitiveComponent* from interface to USkeletalMeshComponent* for AnimInstance access
	if (USkeletalMeshComponent* TPSMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* AnimInst = TPSMesh->GetAnimInstance())
		{
			if (AnimInst->Montage_IsPlaying(Montage))
			{
				AnimInst->Montage_Stop(0.2f, Montage);
			}
		}
	}
}

void UPumpActionFireComponent::ReattachWeaponToSocket(bool bToReloadSocket)
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>())
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("ReattachWeaponToSocket(bool) - No WeaponActor or doesn't implement HoldableInterface!"));
		return;
	}

	// Resolve socket name from weapon interface and delegate to FName overload
	FName SocketName = bToReloadSocket
		? IHoldableInterface::Execute_GetReloadAttachSocket(WeaponActor)
		: IHoldableInterface::Execute_GetAttachSocket(WeaponActor);

	ReattachWeaponToSocket(SocketName);
}

void UPumpActionFireComponent::ReattachWeaponToSocket(FName SocketName)
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor)
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("ReattachWeaponToSocket - No WeaponActor!"));
		return;
	}

	AActor* CharacterActor = WeaponActor->GetOwner();
	if (!CharacterActor)
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("ReattachWeaponToSocket - No CharacterActor (weapon has no owner)!"));
		return;
	}

	if (!WeaponActor->Implements<UHoldableInterface>())
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("ReattachWeaponToSocket - Weapon doesn't implement HoldableInterface!"));
		return;
	}
	if (!CharacterActor->Implements<UCharacterMeshProviderInterface>())
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("ReattachWeaponToSocket - Character doesn't implement CharacterMeshProviderInterface!"));
		return;
	}

	// Get character meshes
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);

	// Get weapon meshes
	UPrimitiveComponent* FPSWeaponMesh = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSWeaponMesh = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	UE_LOG(LogPumpActionFire, Log, TEXT("ReattachWeaponToSocket - Socket: %s, Character: %s, IsLocallyControlled: %s"),
		*SocketName.ToString(),
		*CharacterActor->GetName(),
		CharacterActor->GetInstigatorController() && CharacterActor->GetInstigatorController()->IsLocalController() ? TEXT("true") : TEXT("false"));

	UE_LOG(LogPumpActionFire, Log, TEXT("  BodyMesh: %s, ArmsMesh: %s, FPSWeapon: %s, TPSWeapon: %s"),
		BodyMesh ? *BodyMesh->GetName() : TEXT("NULL"),
		ArmsMesh ? *ArmsMesh->GetName() : TEXT("NULL"),
		FPSWeaponMesh ? *FPSWeaponMesh->GetName() : TEXT("NULL"),
		TPSWeaponMesh ? *TPSWeaponMesh->GetName() : TEXT("NULL"));

	// Re-attach FPS weapon mesh to Arms
	if (FPSWeaponMesh && ArmsMesh)
	{
		FPSWeaponMesh->AttachToComponent(
			ArmsMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			SocketName
		);
		FPSWeaponMesh->SetRelativeTransform(FTransform::Identity);
		UE_LOG(LogPumpActionFire, Log, TEXT("  FPS weapon attached to Arms socket: %s"), *SocketName.ToString());
	}
	else
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("  Cannot attach FPS weapon - FPSWeaponMesh: %s, ArmsMesh: %s"),
			FPSWeaponMesh ? TEXT("valid") : TEXT("NULL"),
			ArmsMesh ? TEXT("valid") : TEXT("NULL"));
	}

	// Re-attach TPS weapon mesh to Body
	if (TPSWeaponMesh && BodyMesh)
	{
		TPSWeaponMesh->AttachToComponent(
			BodyMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			SocketName
		);
		TPSWeaponMesh->SetRelativeTransform(FTransform::Identity);
		UE_LOG(LogPumpActionFire, Log, TEXT("  TPS weapon attached to Body socket: %s"), *SocketName.ToString());
	}
	else
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("  Cannot attach TPS weapon - TPSWeaponMesh: %s, BodyMesh: %s"),
			TPSWeaponMesh ? TEXT("valid") : TEXT("NULL"),
			BodyMesh ? TEXT("valid") : TEXT("NULL"));
	}
}

void UPumpActionFireComponent::SetPumpActionState(bool bPumping, bool bChamberIsEmpty)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogPumpActionFire, Warning, TEXT("SetPumpActionState - Not authority, ignoring!"));
		return;
	}

	UE_LOG(LogPumpActionFire, Log, TEXT("[Server] SetPumpActionState - bPumping: %s, bChamberIsEmpty: %s"),
		bPumping ? TEXT("true") : TEXT("false"),
		bChamberIsEmpty ? TEXT("true") : TEXT("false"));

	bIsPumping = bPumping;
	bChamberEmpty = bChamberIsEmpty;

	PropagateStateToAnimInstances();
}
