// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoltActionReloadComponent.h"
#include "Components/BoltActionFireComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBoltActionReload, Log, All);

UBoltActionReloadComponent::UBoltActionReloadComponent()
{
	// Bolt-action rifles need weapon repositioning during reload
	bReattachDuringReload = true;
}

// ============================================
// VIRTUAL OVERRIDES
// ============================================

bool UBoltActionReloadComponent::CanReload_Internal() const
{
	// First check base conditions (not reloading, magazine not full, etc.)
	if (!Super::CanReload_Internal())
	{
		return false;
	}

	// Bolt-action specific: Cannot reload while bolt is cycling
	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	if (BoltComp && BoltComp->IsCyclingBolt())
	{
		UE_LOG(LogBoltActionReload, Verbose, TEXT("CanReload_Internal: false - bolt is cycling"));
		return false;
	}

	// Cannot reload while bolt-action pending after shoot
	if (BoltComp && BoltComp->IsBoltActionPendingAfterShoot())
	{
		UE_LOG(LogBoltActionReload, Verbose, TEXT("CanReload_Internal: false - bolt-action pending after shoot"));
		return false;
	}

	return true;
}

void UBoltActionReloadComponent::PlayReloadMontages()
{
	UE_LOG(LogBoltActionReload, Log, TEXT("PlayReloadMontages - bReattachDuringReload: %s"),
		bReattachDuringReload ? TEXT("true") : TEXT("false"));

	// Re-attach weapon to reload socket if configured
	// Uses BoltActionFireComponent's shared implementation to avoid code duplication
	if (bReattachDuringReload)
	{
		UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
		if (BoltComp)
		{
			AActor* WeaponActor = GetOwnerItem();
			if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
			{
				FName ReloadSocket = IHoldableInterface::Execute_GetReloadAttachSocket(WeaponActor);
				UE_LOG(LogBoltActionReload, Log, TEXT("Reattaching weapon to reload socket: %s"), *ReloadSocket.ToString());
				BoltComp->ReattachWeaponToSocket(ReloadSocket);
			}
		}
	}

	// Call parent to play reload montages
	Super::PlayReloadMontages();
}

void UBoltActionReloadComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Check if bolt-action is in progress (works on both server and clients via replicated state)
	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	bool bBoltActionInProgress = bBoltActionPending || (BoltComp && BoltComp->IsCyclingBolt());

	UE_LOG(LogBoltActionReload, Log, TEXT("OnMontageEnded (Reload) - bInterrupted: %s, Authority: %s, bBoltActionPending: %s, bIsCyclingBolt: %s"),
		bInterrupted ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
		bBoltActionPending ? TEXT("true") : TEXT("false"),
		BoltComp && BoltComp->IsCyclingBolt() ? TEXT("true") : TEXT("false"));

	// If interrupted, handle cleanup
	if (bInterrupted)
	{
		UE_LOG(LogBoltActionReload, Log, TEXT("Reload interrupted - cleaning up"));

		// Re-attach weapon to original socket (only if bolt-action NOT in progress)
		// If bolt-action is in progress, weapon stays on weapon_l for bolt-action sequence
		if (bReattachDuringReload && !bBoltActionInProgress && BoltComp)
		{
			BoltComp->ReattachWeaponToSocket(false);  // Back to equip socket
		}

		// Reset state (only if bolt-action NOT in progress) - SERVER ONLY
		if (GetOwner() && GetOwner()->HasAuthority() && !bBoltActionInProgress)
		{
			bIsReloading = false;
		}
		return;
	}

	// Reload montage completed normally
	// If bolt-action is in progress, don't do anything - weapon stays on weapon_l
	// The bolt-action sequence will complete and handle reattachment via OnRep_IsCyclingBolt
	if (bBoltActionInProgress)
	{
		UE_LOG(LogBoltActionReload, Log, TEXT("Reload montage ended - bolt-action in progress, weapon stays on weapon_l"));
		return;
	}

	// If no bolt-action in progress (notify wasn't triggered), complete reload immediately
	// This is fallback for weapons without AnimNotify_ReloadBoltAction in their reload montage
	UE_LOG(LogBoltActionReload, Log, TEXT("Reload montage ended - no bolt-action in progress, completing reload"));

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// Re-attach weapon to original socket
		if (bReattachDuringReload && BoltComp)
		{
			BoltComp->ReattachWeaponToSocket(false);  // Back to equip socket
		}

		// Complete reload
		Super::OnReloadComplete();
	}
}

void UBoltActionReloadComponent::StopReloadMontages()
{
	// Call parent to stop montages
	Super::StopReloadMontages();

	// Re-attach weapon to original socket using BoltActionFireComponent's shared implementation
	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	if (bReattachDuringReload && BoltComp)
	{
		BoltComp->ReattachWeaponToSocket(false);  // Back to equip socket
	}

	// Also stop any bolt-action montages if pending
	if (bBoltActionPending && BoltComp)
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
		bBoltActionPending = false;
	}
}

// ============================================
// BOLT-ACTION INTEGRATION
// ============================================

void UBoltActionReloadComponent::OnReloadBoltActionNotify()
{
	UE_LOG(LogBoltActionReload, Log, TEXT("OnReloadBoltActionNotify - Authority: %s, bBoltActionPending: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
		bBoltActionPending ? TEXT("true") : TEXT("false"));

	// Prevent double-triggering
	if (bBoltActionPending)
	{
		UE_LOG(LogBoltActionReload, Warning, TEXT("OnReloadBoltActionNotify - Bolt-action already pending, ignoring"));
		return;
	}

	// SERVER ONLY: Set state and trigger bolt-action sequence
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogBoltActionReload, Log, TEXT("OnReloadBoltActionNotify - Not authority, skipping (clients will get state via OnRep)"));
		return;
	}

	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	if (!BoltComp)
	{
		UE_LOG(LogBoltActionReload, Warning, TEXT("OnReloadBoltActionNotify - No BoltActionFireComponent found"));
		return;
	}

	UE_LOG(LogBoltActionReload, Log, TEXT("[Server] AnimNotify triggered - starting bolt-action to chamber round"));

	bBoltActionPending = true;

	// Set bolt-action state via encapsulated method (will replicate to clients)
	// Using SetBoltActionState() instead of direct property access for proper encapsulation
	BoltComp->SetBoltActionState(true, false);  // bCycling=true, bChamberEmpty=false (chambering from reload)

	// Get character meshes to play bolt-action montage
	AActor* CharacterActor = GetOwnerCharacterActor();
	if (CharacterActor && CharacterActor->Implements<UCharacterMeshProviderInterface>())
	{
		USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
		USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
		USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

		// Play bolt-action montage on character meshes
		if (BoltComp->BoltActionMontage)
		{
			if (BodyMesh)
			{
				if (UAnimInstance* AnimInst = BodyMesh->GetAnimInstance())
				{
					AnimInst->Montage_Play(BoltComp->BoltActionMontage);

					// Bind OUR delegate to handle reload completion
					FOnMontageEnded EndDelegate;
					EndDelegate.BindUObject(this, &UBoltActionReloadComponent::OnBoltActionMontageEnded);
					AnimInst->Montage_SetEndDelegate(EndDelegate, BoltComp->BoltActionMontage);
				}
			}

			if (ArmsMesh)
			{
				if (UAnimInstance* AnimInst = ArmsMesh->GetAnimInstance())
				{
					AnimInst->Montage_Play(BoltComp->BoltActionMontage);
				}
			}

			if (LegsMesh)
			{
				if (UAnimInstance* AnimInst = LegsMesh->GetAnimInstance())
				{
					AnimInst->Montage_Play(BoltComp->BoltActionMontage);
				}
			}

			UE_LOG(LogBoltActionReload, Log, TEXT("[Server] Bolt-action montage started"));
		}
		else
		{
			UE_LOG(LogBoltActionReload, Warning, TEXT("[Server] No BoltActionMontage assigned - completing reload immediately"));
			OnBoltActionAfterReloadComplete();
		}
	}
}

void UBoltActionReloadComponent::OnBoltActionAfterReloadComplete()
{
	UE_LOG(LogBoltActionReload, Log, TEXT("OnBoltActionAfterReloadComplete - Authority: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	bBoltActionPending = false;

	// Complete bolt-action sequence - this resets bIsCyclingBolt AND handles weapon reattachment
	UBoltActionFireComponent* BoltComp = GetBoltActionFireComponent();
	if (BoltComp)
	{
		UE_LOG(LogBoltActionReload, Log, TEXT("Calling BoltComp->OnBoltActionComplete() to reset bolt state"));
		BoltComp->OnBoltActionComplete();
	}
	else
	{
		// Fallback: No BoltComp - this should not happen in normal operation
		// Log warning but continue with reload completion
		UE_LOG(LogBoltActionReload, Warning, TEXT("No BoltActionFireComponent found - cannot reattach weapon!"));
	}

	// Complete reload using parent implementation (SERVER ONLY)
	// Handles: ammo transfer, OnWeaponReloadComplete callback, bIsReloading = false
	Super::OnReloadComplete();
}

// ============================================
// INTERNAL HELPERS
// ============================================

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
