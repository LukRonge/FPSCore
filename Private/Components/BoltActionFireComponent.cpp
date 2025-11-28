// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoltActionFireComponent.h"
#include "Components/BallisticsComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

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
	PropagateStateToAnimInstances();

	// If cycling started on client, play montages
	if (bIsCyclingBolt)
	{
		PlayBoltActionMontages();
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
		return false;
	}

	if (bChamberEmpty)
	{
		return false;
	}

	// Base class checks (ammo, BallisticsComponent, etc.)
	return Super::CanFire();
}

void UBoltActionFireComponent::TriggerPulled()
{
	if (CanFire() && !bTriggerHeld)
	{
		bTriggerHeld = true;
		Fire();

		// After firing, start bolt-action sequence (SERVER ONLY)
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			StartBoltAction(false);
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

	// SERVER: Set state and play montages
	if (GetOwner()->HasAuthority())
	{
		bIsCyclingBolt = true;

		// If from reload, chamber is being loaded
		if (bFromReload)
		{
			bChamberEmpty = false;
		}

		PropagateStateToAnimInstances();
	}

	// Play montages (LOCAL - runs on server immediately, clients via OnRep)
	PlayBoltActionMontages();
}

void UBoltActionFireComponent::OnBoltActionComplete()
{
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
			}
		}

		PropagateStateToAnimInstances();
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
