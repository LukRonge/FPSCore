// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoltActionReloadComponent.h"
#include "Components/BoltActionFireComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UBoltActionReloadComponent::UBoltActionReloadComponent()
{
	// Bolt-action rifles need weapon repositioning during reload
	bReattachDuringReload = true;
}

// ============================================
// VIRTUAL OVERRIDES
// ============================================

void UBoltActionReloadComponent::PlayReloadMontages()
{
	// Re-attach weapon to reload socket if configured
	if (bReattachDuringReload)
	{
		AActor* WeaponActor = GetOwnerItem();
		if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
		{
			FName ReloadSocket = IHoldableInterface::Execute_GetReloadAttachSocket(WeaponActor);
			ReattachWeaponToSocket(ReloadSocket);
		}
	}

	// Call parent to play reload montages
	Super::PlayReloadMontages();
}

void UBoltActionReloadComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// If interrupted, handle cleanup
	if (bInterrupted)
	{
		// Re-attach weapon to original socket
		if (bReattachDuringReload)
		{
			AActor* WeaponActor = GetOwnerItem();
			if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
			{
				FName EquipSocket = IHoldableInterface::Execute_GetAttachSocket(WeaponActor);
				ReattachWeaponToSocket(EquipSocket);
			}
		}

		// Reset state
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			bIsReloading = false;
		}
		return;
	}

	// Reload montage completed normally
	// Now trigger bolt-action to chamber first round

	// SERVER: Start bolt-action sequence
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
		if (BoltComp)
		{
			bBoltActionPending = true;
			BoltComp->StartBoltAction(true);  // true = from reload

			// Get character Body mesh to bind montage end delegate
			AActor* CharacterActor = GetOwnerCharacterActor();
			if (CharacterActor && CharacterActor->Implements<UCharacterMeshProviderInterface>())
			{
				USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
				if (BodyMesh)
				{
					if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
					{
						// Bind to bolt-action montage end
						if (BoltComp->BoltActionMontage)
						{
							FOnMontageEnded EndDelegate;
							EndDelegate.BindUObject(this, &UBoltActionReloadComponent::OnBoltActionMontageEnded);
							AnimInst->Montage_SetEndDelegate(EndDelegate, BoltComp->BoltActionMontage);
						}
					}
				}
			}
		}
		else
		{
			// No bolt-action component - complete reload immediately
			OnBoltActionAfterReloadComplete();
		}
	}
}

void UBoltActionReloadComponent::StopReloadMontages()
{
	// Call parent to stop montages
	Super::StopReloadMontages();

	// Re-attach weapon to original socket
	if (bReattachDuringReload)
	{
		AActor* WeaponActor = GetOwnerItem();
		if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
		{
			FName EquipSocket = IHoldableInterface::Execute_GetAttachSocket(WeaponActor);
			ReattachWeaponToSocket(EquipSocket);
		}
	}

	// Also stop any bolt-action montages if pending
	if (bBoltActionPending)
	{
		UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
		if (BoltComp)
		{
			// Stop bolt-action montages (character and weapon)
			AActor* CharacterActor = GetOwnerCharacterActor();
			if (CharacterActor && CharacterActor->Implements<UCharacterMeshProviderInterface>())
			{
				if (BoltComp->BoltActionMontage)
				{
					USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
					USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
					USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

					if (BodyMesh)
					{
						if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
						{
							AnimInst->Montage_Stop(0.2f, BoltComp->BoltActionMontage);
						}
					}
					if (ArmsMesh)
					{
						if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
						{
							AnimInst->Montage_Stop(0.2f, BoltComp->BoltActionMontage);
						}
					}
					if (LegsMesh)
					{
						if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
						{
							AnimInst->Montage_Stop(0.2f, BoltComp->BoltActionMontage);
						}
					}
				}
			}
		}
		bBoltActionPending = false;
	}
}

// ============================================
// BOLT-ACTION INTEGRATION
// ============================================

void UBoltActionReloadComponent::OnBoltActionAfterReloadComplete()
{
	bBoltActionPending = false;

	// Re-attach weapon back to equip socket
	if (bReattachDuringReload)
	{
		AActor* WeaponActor = GetOwnerItem();
		if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
		{
			FName EquipSocket = IHoldableInterface::Execute_GetAttachSocket(WeaponActor);
			ReattachWeaponToSocket(EquipSocket);
		}
	}

	// Reset bolt-action fire component chamber state
	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	if (BoltComp)
	{
		BoltComp->ResetChamberState();
	}

	// Complete reload using parent implementation (SERVER ONLY)
	// Handles: ammo transfer, OnWeaponReloadComplete callback, bIsReloading = false
	Super::OnReloadComplete();
}

// ============================================
// INTERNAL HELPERS
// ============================================

void UBoltActionReloadComponent::ReattachWeaponToSocket(FName SocketName)
{
	AActor* WeaponActor = GetOwnerItem();
	AActor* CharacterActor = GetOwnerCharacterActor();

	if (!WeaponActor || !CharacterActor) return;
	if (!WeaponActor->Implements<UHoldableInterface>()) return;
	if (!CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);

	UPrimitiveComponent* FPSWeaponMesh = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSWeaponMesh = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	// Re-attach FPS weapon mesh to Arms
	if (FPSWeaponMesh && ArmsMesh)
	{
		FPSWeaponMesh->AttachToComponent(
			ArmsMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			SocketName
		);
		FPSWeaponMesh->SetRelativeTransform(FTransform::Identity);
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
	}
}

UBoltActionFireComponent* UBoltActionReloadComponent::GetBoltActionFireComponent() const
{
	AActor* WeaponActor = GetOwnerItem();
	if (!WeaponActor) return nullptr;

	return WeaponActor->FindComponentByClass<UBoltActionFireComponent>();
}

void UBoltActionReloadComponent::OnBoltActionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bBoltActionPending) return;

	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	if (!BoltComp || Montage != BoltComp->BoltActionMontage) return;

	// Bolt-action after reload completed
	OnBoltActionAfterReloadComplete();
}
