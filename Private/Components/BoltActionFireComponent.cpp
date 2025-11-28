// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoltActionFireComponent.h"
#include "Components/BallisticsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogBoltActionFire, Log, All);

UBoltActionFireComponent::UBoltActionFireComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Bolt-action defaults
	FireRate = 40.0f;  // ~40 RPM theoretical max (bolt-action limited)
	SpreadScale = 0.3f;  // High accuracy (sniper rifle)
	RecoilScale = 1.5f;  // Higher recoil (high-powered rifle)
}

void UBoltActionFireComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBoltActionFireComponent, bIsCyclingBolt);
	DOREPLIFETIME(UBoltActionFireComponent, bChamberEmpty);
}

// ============================================
// ONREP CALLBACKS
// ============================================

void UBoltActionFireComponent::OnRep_IsCyclingBolt()
{
	UE_LOG(LogBoltActionFire, Log, TEXT("[Client] OnRep_IsCyclingBolt: %s"), bIsCyclingBolt ? TEXT("true") : TEXT("false"));

	PropagateStateToAnimInstances();

	// If cycling started on client, reattach weapon and play montages
	if (bIsCyclingBolt)
	{
		ReattachWeaponToSocket(true);  // To reload socket (weapon_l)
		PlayBoltActionMontages();
	}
	else
	{
		// Bolt-action ended - reattach weapon back to normal socket
		ReattachWeaponToSocket(false);  // To equip socket (weapon_r)
	}
}

void UBoltActionFireComponent::OnRep_ChamberEmpty()
{
	PropagateStateToAnimInstances();
}

void UBoltActionFireComponent::PropagateStateToAnimInstances()
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>()) return;

	// Get weapon meshes and update their AnimInstances
	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	if (USkeletalMeshComponent* FPSMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			AnimInst->NativeUpdateAnimation(0.0f);
		}
	}

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

bool UBoltActionFireComponent::CanFire() const
{
	// Bolt-action specific checks
	if (bIsCyclingBolt)
	{
		UE_LOG(LogBoltActionFire, Verbose, TEXT("CanFire: false - bIsCyclingBolt is true"));
		return false;
	}

	if (bChamberEmpty)
	{
		UE_LOG(LogBoltActionFire, Verbose, TEXT("CanFire: false - bChamberEmpty is true"));
		return false;
	}

	// Base class checks (ammo, BallisticsComponent, etc.)
	bool bCanFire = Super::CanFire();
	UE_LOG(LogBoltActionFire, Verbose, TEXT("CanFire: %s (base check)"), bCanFire ? TEXT("true") : TEXT("false"));
	return bCanFire;
}

void UBoltActionFireComponent::TriggerPulled()
{
	UE_LOG(LogBoltActionFire, Log, TEXT("TriggerPulled - CanFire: %s, bTriggerHeld: %s, Authority: %s"),
		CanFire() ? TEXT("true") : TEXT("false"),
		bTriggerHeld ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	if (CanFire() && !bTriggerHeld)
	{
		bTriggerHeld = true;
		Fire();

		// NOTE: Bolt-action sequence is NOT started here immediately!
		// It will be started after ShootMontage ends via OnShootMontageEnded callback
		// The weapon class (Sako85) registers the delegate in Multicast_PlayShootEffects
		// This ensures proper animation sequencing: shoot recoil -> bolt manipulation

		// Mark that bolt-action is pending (SERVER ONLY)
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			bBoltActionPendingAfterShoot = true;
			UE_LOG(LogBoltActionFire, Log, TEXT("[Server] Fire complete - bolt-action pending after shoot montage"));
		}
	}
}

void UBoltActionFireComponent::TriggerReleased()
{
	bTriggerHeld = false;
}

// ============================================
// BOLT-ACTION API
// ============================================

void UBoltActionFireComponent::StartBoltAction(bool bFromReload)
{
	if (!GetOwner()) return;

	UE_LOG(LogBoltActionFire, Log, TEXT("StartBoltAction - bFromReload: %s, Authority: %s"),
		bFromReload ? TEXT("true") : TEXT("false"),
		GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// SERVER: Set state
	if (GetOwner()->HasAuthority())
	{
		bIsCyclingBolt = true;

		// If from reload, chamber is being loaded
		if (bFromReload)
		{
			bChamberEmpty = false;
		}

		PropagateStateToAnimInstances();

		// Reattach weapon to reload socket (weapon_l) for bolt manipulation
		ReattachWeaponToSocket(true);
	}

	// Play montages (LOCAL - runs on server immediately, clients via OnRep)
	PlayBoltActionMontages();
}

void UBoltActionFireComponent::OnBoltActionComplete()
{
	UE_LOG(LogBoltActionFire, Log, TEXT("OnBoltActionComplete - Authority: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// SERVER: Update state
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bIsCyclingBolt = false;

		// Check if magazine is empty after chambering
		AActor* WeaponActor = GetOwner();
		if (WeaponActor && WeaponActor->Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(WeaponActor);
			if (CurrentAmmo <= 0)
			{
				bChamberEmpty = true;
				UE_LOG(LogBoltActionFire, Log, TEXT("[Server] Chamber empty - no ammo to chamber"));
			}
		}

		PropagateStateToAnimInstances();

		// Reattach weapon back to equip socket (weapon_r)
		ReattachWeaponToSocket(false);

		UE_LOG(LogBoltActionFire, Log, TEXT("[Server] Bolt-action complete - bIsCyclingBolt: false, bChamberEmpty: %s"),
			bChamberEmpty ? TEXT("true") : TEXT("false"));
	}
}

void UBoltActionFireComponent::OnBoltActionStart()
{
	// LOCAL operation - start weapon bolt-action montage
	// Called by AnimNotify_BoltActionStart when character hand grabs bolt handle
	// This synchronizes weapon bolt animation with character hand movement

	PlayWeaponMontage(WeaponBoltActionMontage);
}

void UBoltActionFireComponent::OnShellEject()
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

void UBoltActionFireComponent::OnChamberRound()
{
	// LOCAL visual feedback + SERVER state check
	// The actual ammo consumption happened during Fire()
	// This notify is for audio/visual feedback of bolt closing

	// SERVER: Check if we have ammo to chamber
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		AActor* WeaponActor = GetOwner();
		if (WeaponActor && WeaponActor->Implements<UAmmoConsumerInterface>())
		{
			int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(WeaponActor);
			if (CurrentAmmo <= 0)
			{
				// No ammo to chamber - will be set in OnBoltActionComplete
				// This prevents the "click" of chambering when empty
			}
		}
	}
}

void UBoltActionFireComponent::ResetChamberState()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bChamberEmpty = false;
		PropagateStateToAnimInstances();
	}
}

void UBoltActionFireComponent::OnShootMontageEnded()
{
	UE_LOG(LogBoltActionFire, Log, TEXT("OnShootMontageEnded - bBoltActionPendingAfterShoot: %s, Authority: %s"),
		bBoltActionPendingAfterShoot ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// SERVER ONLY: Start bolt-action if pending
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bBoltActionPendingAfterShoot)
	{
		bBoltActionPendingAfterShoot = false;
		UE_LOG(LogBoltActionFire, Log, TEXT("[Server] Shoot montage ended - starting bolt-action sequence"));
		StartBoltAction(false);
	}
}

// ============================================
// INTERNAL HELPERS
// ============================================

void UBoltActionFireComponent::PlayBoltActionMontages()
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor) return;

	AActor* CharacterActor = WeaponActor->GetOwner();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	// Play on character meshes (Body, Arms, Legs)
	// NOTE: Weapon montage is NOT started here - it's triggered by AnimNotify_BoltActionStart
	// when character's hand reaches the bolt handle for perfect synchronization
	if (BoltActionMontage)
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
		USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

		if (BodyMesh)
		{
			if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(BoltActionMontage);

				// Bind end delegate only on Body mesh (single callback)
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UBoltActionFireComponent::OnMontageEnded);
				AnimInst->Montage_SetEndDelegate(EndDelegate, BoltActionMontage);
			}
		}

		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(BoltActionMontage);
			}
		}

		if (LegsMesh)
		{
			if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(BoltActionMontage);
			}
		}
	}

	// NOTE: Weapon bolt-action montage is triggered by AnimNotify_BoltActionStart
	// in character montage for synchronized hand/bolt animation
}

void UBoltActionFireComponent::StopBoltActionMontages()
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor) return;

	AActor* CharacterActor = WeaponActor->GetOwner();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	if (BoltActionMontage)
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
		USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

		if (BodyMesh)
		{
			if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(BoltActionMontage))
				{
					AnimInst->Montage_Stop(0.2f, BoltActionMontage);
				}
			}
		}

		if (ArmsMesh)
		{
			if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(BoltActionMontage))
				{
					AnimInst->Montage_Stop(0.2f, BoltActionMontage);
				}
			}
		}

		if (LegsMesh)
		{
			if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(BoltActionMontage))
				{
					AnimInst->Montage_Stop(0.2f, BoltActionMontage);
				}
			}
		}
	}

	// Also stop weapon bolt-action montage
	StopWeaponMontage(WeaponBoltActionMontage);
}

void UBoltActionFireComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != BoltActionMontage) return;

	// Both server and clients handle montage end
	OnBoltActionComplete();
}

void UBoltActionFireComponent::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	if (USkeletalMeshComponent* FPSMesh = Cast<USkeletalMeshComponent>(FPSMeshComp))
	{
		if (UAnimInstance* AnimInst = FPSMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(Montage);
		}
	}

	if (USkeletalMeshComponent* TPSMesh = Cast<USkeletalMeshComponent>(TPSMeshComp))
	{
		if (UAnimInstance* AnimInst = TPSMesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(Montage);
		}
	}
}

void UBoltActionFireComponent::StopWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>()) return;

	UPrimitiveComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

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

void UBoltActionFireComponent::ReattachWeaponToSocket(bool bToReloadSocket)
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>())
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("ReattachWeaponToSocket(bool) - No WeaponActor or doesn't implement HoldableInterface!"));
		return;
	}

	// Resolve socket name from weapon interface and delegate to FName overload
	FName SocketName = bToReloadSocket
		? IHoldableInterface::Execute_GetReloadAttachSocket(WeaponActor)
		: IHoldableInterface::Execute_GetAttachSocket(WeaponActor);

	ReattachWeaponToSocket(SocketName);
}

void UBoltActionFireComponent::ReattachWeaponToSocket(FName SocketName)
{
	AActor* WeaponActor = GetOwner();
	if (!WeaponActor)
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("ReattachWeaponToSocket - No WeaponActor!"));
		return;
	}

	AActor* CharacterActor = WeaponActor->GetOwner();
	if (!CharacterActor)
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("ReattachWeaponToSocket - No CharacterActor (weapon has no owner)!"));
		return;
	}

	if (!WeaponActor->Implements<UHoldableInterface>())
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("ReattachWeaponToSocket - Weapon doesn't implement HoldableInterface!"));
		return;
	}
	if (!CharacterActor->Implements<UCharacterMeshProviderInterface>())
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("ReattachWeaponToSocket - Character doesn't implement CharacterMeshProviderInterface!"));
		return;
	}

	// Get character meshes
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);

	// Get weapon meshes
	UPrimitiveComponent* FPSWeaponMesh = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSWeaponMesh = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	UE_LOG(LogBoltActionFire, Log, TEXT("ReattachWeaponToSocket - Socket: %s, Character: %s, IsLocallyControlled: %s"),
		*SocketName.ToString(),
		*CharacterActor->GetName(),
		CharacterActor->GetInstigatorController() && CharacterActor->GetInstigatorController()->IsLocalController() ? TEXT("true") : TEXT("false"));

	UE_LOG(LogBoltActionFire, Log, TEXT("  BodyMesh: %s, ArmsMesh: %s, FPSWeapon: %s, TPSWeapon: %s"),
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
		UE_LOG(LogBoltActionFire, Log, TEXT("  FPS weapon attached to Arms socket: %s"), *SocketName.ToString());
	}
	else
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("  Cannot attach FPS weapon - FPSWeaponMesh: %s, ArmsMesh: %s"),
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
		UE_LOG(LogBoltActionFire, Log, TEXT("  TPS weapon attached to Body socket: %s"), *SocketName.ToString());
	}
	else
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("  Cannot attach TPS weapon - TPSWeaponMesh: %s, BodyMesh: %s"),
			TPSWeaponMesh ? TEXT("valid") : TEXT("NULL"),
			BodyMesh ? TEXT("valid") : TEXT("NULL"));
	}
}

void UBoltActionFireComponent::SetBoltActionState(bool bCycling, bool bChamberIsEmpty)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogBoltActionFire, Warning, TEXT("SetBoltActionState - Not authority, ignoring!"));
		return;
	}

	UE_LOG(LogBoltActionFire, Log, TEXT("[Server] SetBoltActionState - bCycling: %s, bChamberIsEmpty: %s"),
		bCycling ? TEXT("true") : TEXT("false"),
		bChamberIsEmpty ? TEXT("true") : TEXT("false"));

	bIsCyclingBolt = bCycling;
	bChamberEmpty = bChamberIsEmpty;

	PropagateStateToAnimInstances();
}
