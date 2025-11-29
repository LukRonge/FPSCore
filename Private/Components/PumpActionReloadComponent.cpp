// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/PumpActionReloadComponent.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/AmmoProviderInterface.h"
#include "Interfaces/ReloadableInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogPumpActionReload, Log, All);

UPumpActionReloadComponent::UPumpActionReloadComponent()
{
	// Shotguns need weapon repositioning during reload
	bReattachDuringReload = true;
	SetIsReplicatedByDefault(true);
}

void UPumpActionReloadComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPumpActionReloadComponent, bChamberEmpty);
	DOREPLIFETIME(UPumpActionReloadComponent, bIsPumping);
	DOREPLIFETIME(UPumpActionReloadComponent, bNeedsPumpAfterReload);
}

// ============================================
// ONREP CALLBACKS
// ============================================

void UPumpActionReloadComponent::OnRep_ChamberEmpty()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("[Client] OnRep_ChamberEmpty: %s"),
		bChamberEmpty ? TEXT("true") : TEXT("false"));
}

void UPumpActionReloadComponent::OnRep_IsPumping()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("[Client] OnRep_IsPumping: %s"),
		bIsPumping ? TEXT("true") : TEXT("false"));

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

void UPumpActionReloadComponent::OnRep_NeedsPumpAfterReload()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("[Client] OnRep_NeedsPumpAfterReload: %s"),
		bNeedsPumpAfterReload ? TEXT("true") : TEXT("false"));
}

// ============================================
// STATE API
// ============================================

void UPumpActionReloadComponent::ResetChamberState()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bChamberEmpty = false;
		bIsPumping = false;
		bNeedsPumpAfterReload = false;
		UE_LOG(LogPumpActionReload, Log, TEXT("[Server] ResetChamberState - all states reset"));
	}
}

void UPumpActionReloadComponent::SetChamberEmpty(bool bEmpty)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bChamberEmpty = bEmpty;
		UE_LOG(LogPumpActionReload, Log, TEXT("[Server] SetChamberEmpty: %s"),
			bChamberEmpty ? TEXT("true") : TEXT("false"));
	}
}

// ============================================
// VIRTUAL OVERRIDES
// ============================================

bool UPumpActionReloadComponent::CanReload_Internal() const
{
	// First check base conditions (not reloading, magazine not full, etc.)
	if (!Super::CanReload_Internal())
	{
		return false;
	}

	// Cannot reload while pump is cycling
	if (bIsPumping)
	{
		UE_LOG(LogPumpActionReload, Verbose, TEXT("CanReload_Internal: false - pump is cycling"));
		return false;
	}

	return true;
}

void UPumpActionReloadComponent::PlayReloadMontages()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("PlayReloadMontages - bReattachDuringReload: %s"),
		bReattachDuringReload ? TEXT("true") : TEXT("false"));

	// Reset guards for this reload cycle
	bShellInsertedThisReload = false;
	bShellGrabbedThisReload = false;

	// Check if chamber is empty when starting reload
	// This determines if we need pump-action after first shell
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bChamberEmpty)
		{
			bNeedsPumpAfterReload = true;
			UE_LOG(LogPumpActionReload, Log, TEXT("[Server] Chamber empty - will need pump-action after first shell"));
		}
		else
		{
			bNeedsPumpAfterReload = false;
		}
	}

	// Re-attach weapon to reload socket if configured
	if (bReattachDuringReload)
	{
		AActor* WeaponActor = GetOwnerItem();
		if (WeaponActor && WeaponActor->Implements<UHoldableInterface>())
		{
			FName ReloadSocket = IHoldableInterface::Execute_GetReloadAttachSocket(WeaponActor);
			UE_LOG(LogPumpActionReload, Log, TEXT("Reattaching weapon to reload socket: %s"), *ReloadSocket.ToString());
			ReattachWeaponToSocket(ReloadSocket);
		}
	}

	// Call parent to play reload montages
	Super::PlayReloadMontages();
}

void UPumpActionReloadComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Check if pump-action is in progress
	bool bPumpActionInProgress = bPumpActionPending || bIsPumping;

	UE_LOG(LogPumpActionReload, Log, TEXT("OnMontageEnded (Reload) - bInterrupted: %s, Authority: %s, bPumpActionPending: %s, bIsPumping: %s"),
		bInterrupted ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
		bPumpActionPending ? TEXT("true") : TEXT("false"),
		bIsPumping ? TEXT("true") : TEXT("false"));

	// If interrupted, handle cleanup
	if (bInterrupted)
	{
		UE_LOG(LogPumpActionReload, Log, TEXT("Reload interrupted - cleaning up"));

		// Ensure magazine is hidden and reattached to weapon
		HideAndAttachMagazineToWeapon();

		// Re-attach weapon to original socket (only if pump-action NOT in progress)
		if (bReattachDuringReload && !bPumpActionInProgress)
		{
			ReattachWeaponToSocket(false);  // Back to equip socket
		}

		// Reset state (only if pump-action NOT in progress) - SERVER ONLY
		if (GetOwner() && GetOwner()->HasAuthority() && !bPumpActionInProgress)
		{
			bIsReloading = false;
		}
		return;
	}

	// Reload montage completed normally
	// If pump-action is in progress, don't do anything - weapon stays on weapon_l
	if (bPumpActionInProgress)
	{
		UE_LOG(LogPumpActionReload, Log, TEXT("Reload montage ended - pump-action in progress, weapon stays on weapon_l"));
		return;
	}

	// If no pump-action in progress, complete reload
	UE_LOG(LogPumpActionReload, Log, TEXT("Reload montage ended - completing reload"));

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		OnReloadComplete();
	}
}

void UPumpActionReloadComponent::StopReloadMontages()
{
	// Call parent to stop montages
	Super::StopReloadMontages();

	// Ensure magazine is hidden and reattached to weapon
	HideAndAttachMagazineToWeapon();

	// Re-attach weapon to original socket
	if (bReattachDuringReload)
	{
		ReattachWeaponToSocket(false);  // Back to equip socket
	}

	// Also stop any pump-action montages if pending
	if (bPumpActionPending)
	{
		StopPumpActionMontages();
		bPumpActionPending = false;
	}
}

void UPumpActionReloadComponent::OnReloadComplete()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("OnReloadComplete - Authority: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// Re-attach weapon to original socket
	if (bReattachDuringReload)
	{
		ReattachWeaponToSocket(false);  // Back to equip socket
	}

	// Notify weapon that reload completed
	AActor* OwnerItem = GetOwnerItem();
	if (OwnerItem && OwnerItem->Implements<UReloadableInterface>())
	{
		IReloadableInterface::Execute_OnWeaponReloadComplete(OwnerItem);
	}

	// Reset reload state - ready for next shell or firing
	bIsReloading = false;
	bNeedsPumpAfterReload = false;

	UE_LOG(LogPumpActionReload, Log, TEXT("[Server] Reload complete - ready for next action"));
}

// ============================================
// SHELL VISUAL API (Using Magazine)
// ============================================

void UPumpActionReloadComponent::OnGrabShell()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("OnGrabShell - bShellGrabbedThisReload: %s"),
		bShellGrabbedThisReload ? TEXT("true") : TEXT("false"));

	// Guard: AnimNotify fires on all 3 meshes (Body, Arms, Legs)
	// Only process the first call per reload cycle
	if (bShellGrabbedThisReload)
	{
		UE_LOG(LogPumpActionReload, Verbose, TEXT("OnGrabShell - Already processed this reload cycle, skipping"));
		return;
	}
	bShellGrabbedThisReload = true;

	// Show magazine mesh and attach to character hand
	ShowAndAttachMagazineToHand();
}

void UPumpActionReloadComponent::OnShellInsert()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("OnShellInsert - Authority: %s, bShellInsertedThisReload: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
		bShellInsertedThisReload ? TEXT("true") : TEXT("false"));

	// Guard: AnimNotify fires on all 3 meshes (Body, Arms, Legs)
	// Only process the first call per reload cycle
	if (bShellInsertedThisReload)
	{
		UE_LOG(LogPumpActionReload, Verbose, TEXT("OnShellInsert - Already processed this reload cycle, skipping"));
		return;
	}
	bShellInsertedThisReload = true;

	// SERVER ONLY: Add ammo
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		AActor* OwnerItem = GetOwnerItem();
		if (!OwnerItem) return;

		// Add 1 shell to magazine
		if (OwnerItem->Implements<UReloadableInterface>())
		{
			AActor* MagActor = IReloadableInterface::Execute_GetMagazineActor(OwnerItem);
			if (MagActor && MagActor->Implements<UAmmoProviderInterface>())
			{
				IAmmoProviderInterface::Execute_AddAmmoToProvider(MagActor, 1);
				UE_LOG(LogPumpActionReload, Log, TEXT("[Server] Shell inserted - +1 ammo"));

				// Log current ammo state
				if (OwnerItem->Implements<UAmmoConsumerInterface>())
				{
					int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(OwnerItem);
					int32 MaxAmmo = IAmmoConsumerInterface::Execute_GetClipSize(OwnerItem);
					UE_LOG(LogPumpActionReload, Log, TEXT("[Server] Ammo: %d / %d"), CurrentAmmo, MaxAmmo);
				}
			}
		}
	}

	// LOCAL: Hide magazine mesh and reattach to weapon
	HideAndAttachMagazineToWeapon();
}

// ============================================
// PUMP-ACTION INTEGRATION
// ============================================

void UPumpActionReloadComponent::OnReloadPumpActionNotify()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("OnReloadPumpActionNotify - Authority: %s, bPumpActionPending: %s, bNeedsPumpAfterReload: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"),
		bPumpActionPending ? TEXT("true") : TEXT("false"),
		bNeedsPumpAfterReload ? TEXT("true") : TEXT("false"));

	// Only trigger pump-action if chamber was empty
	if (!bNeedsPumpAfterReload)
	{
		UE_LOG(LogPumpActionReload, Log, TEXT("OnReloadPumpActionNotify - Chamber not empty, skipping pump-action"));
		return;
	}

	// Prevent double-triggering
	if (bPumpActionPending)
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("OnReloadPumpActionNotify - Pump-action already pending, ignoring"));
		return;
	}

	// SERVER ONLY: Set state and trigger pump-action sequence
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogPumpActionReload, Log, TEXT("OnReloadPumpActionNotify - Not authority, skipping (clients will get state via OnRep)"));
		return;
	}

	UE_LOG(LogPumpActionReload, Log, TEXT("[Server] AnimNotify triggered - starting pump-action to chamber round"));

	bPumpActionPending = true;
	bIsPumping = true;
	bChamberEmpty = false;  // We're chambering a round now

	// Play pump-action montages
	PlayPumpActionMontages();
}

void UPumpActionReloadComponent::OnPumpActionStart()
{
	// LOCAL operation - start weapon pump-action montage
	// Called by AnimNotify_PumpActionStart when character hand grabs pump handle
	PlayWeaponMontage(WeaponPumpActionMontage);
}

void UPumpActionReloadComponent::OnPumpActionAfterReloadComplete()
{
	UE_LOG(LogPumpActionReload, Log, TEXT("OnPumpActionAfterReloadComplete - Authority: %s"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client"));

	bPumpActionPending = false;

	// SERVER: Reset pump state and reattach weapon
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bIsPumping = false;
		bNeedsPumpAfterReload = false;

		// Re-attach weapon back to equip socket
		ReattachWeaponToSocket(false);

		// Notify weapon that reload completed
		AActor* OwnerItem = GetOwnerItem();
		if (OwnerItem && OwnerItem->Implements<UReloadableInterface>())
		{
			IReloadableInterface::Execute_OnWeaponReloadComplete(OwnerItem);
		}

		bIsReloading = false;
		UE_LOG(LogPumpActionReload, Log, TEXT("[Server] Reload + pump-action complete"));
	}
}

// ============================================
// INTERNAL HELPERS
// ============================================

void UPumpActionReloadComponent::OnPumpActionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bPumpActionPending) return;
	if (Montage != PumpActionMontage) return;

	// Pump-action after reload completed
	OnPumpActionAfterReloadComplete();
}

void UPumpActionReloadComponent::ReattachWeaponToSocket(bool bToReloadSocket)
{
	AActor* WeaponActor = GetOwnerItem();
	if (!WeaponActor || !WeaponActor->Implements<UHoldableInterface>())
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("ReattachWeaponToSocket(bool) - No WeaponActor or doesn't implement HoldableInterface!"));
		return;
	}

	// Resolve socket name from weapon interface and delegate to FName overload
	FName SocketName = bToReloadSocket
		? IHoldableInterface::Execute_GetReloadAttachSocket(WeaponActor)
		: IHoldableInterface::Execute_GetAttachSocket(WeaponActor);

	ReattachWeaponToSocket(SocketName);
}

void UPumpActionReloadComponent::ReattachWeaponToSocket(FName SocketName)
{
	AActor* WeaponActor = GetOwnerItem();
	if (!WeaponActor)
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("ReattachWeaponToSocket - No WeaponActor!"));
		return;
	}

	AActor* CharacterActor = GetOwnerCharacterActor();
	if (!CharacterActor)
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("ReattachWeaponToSocket - No CharacterActor!"));
		return;
	}

	if (!WeaponActor->Implements<UHoldableInterface>())
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("ReattachWeaponToSocket - Weapon doesn't implement HoldableInterface!"));
		return;
	}
	if (!CharacterActor->Implements<UCharacterMeshProviderInterface>())
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("ReattachWeaponToSocket - Character doesn't implement CharacterMeshProviderInterface!"));
		return;
	}

	// Get character meshes
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);

	// Get weapon meshes
	UPrimitiveComponent* FPSWeaponMesh = IHoldableInterface::Execute_GetFPSMeshComponent(WeaponActor);
	UPrimitiveComponent* TPSWeaponMesh = IHoldableInterface::Execute_GetTPSMeshComponent(WeaponActor);

	UE_LOG(LogPumpActionReload, Log, TEXT("ReattachWeaponToSocket - Socket: %s"), *SocketName.ToString());

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

void UPumpActionReloadComponent::PlayPumpActionMontages()
{
	AActor* CharacterActor = GetOwnerCharacterActor();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	if (!PumpActionMontage)
	{
		UE_LOG(LogPumpActionReload, Warning, TEXT("PlayPumpActionMontages - No PumpActionMontage assigned"));
		OnPumpActionAfterReloadComplete();
		return;
	}

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
			EndDelegate.BindUObject(this, &UPumpActionReloadComponent::OnPumpActionMontageEnded);
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

	UE_LOG(LogPumpActionReload, Log, TEXT("Pump-action montages started"));
}

void UPumpActionReloadComponent::StopPumpActionMontages()
{
	AActor* CharacterActor = GetOwnerCharacterActor();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	if (!PumpActionMontage) return;

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

	// Also stop weapon pump-action montage
	StopWeaponMontage(WeaponPumpActionMontage);
}

void UPumpActionReloadComponent::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	AActor* WeaponActor = GetOwnerItem();
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

void UPumpActionReloadComponent::StopWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	AActor* WeaponActor = GetOwnerItem();
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

// ============================================
// MAGAZINE/SHELL VISIBILITY HELPERS
// ============================================

void UPumpActionReloadComponent::ShowAndAttachMagazineToHand()
{
	AActor* OwnerItem = GetOwnerItem();
	AActor* CharacterActor = GetOwnerCharacterActor();

	if (!OwnerItem || !CharacterActor) return;
	if (!OwnerItem->Implements<UReloadableInterface>()) return;
	if (!CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	// Get FPS/TPS magazine meshes from weapon via interface
	UPrimitiveComponent* FPSMagMesh = IReloadableInterface::Execute_GetFPSMagazineMesh(OwnerItem);
	UPrimitiveComponent* TPSMagMesh = IReloadableInterface::Execute_GetTPSMagazineMesh(OwnerItem);

	// Get character meshes for attachment
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);

	UE_LOG(LogPumpActionReload, Log, TEXT("ShowAndAttachMagazineToHand - Socket: %s"), *ShellGrabSocketName.ToString());

	// Show and attach FPS magazine mesh to Arms mesh (visible only to owner)
	if (FPSMagMesh && ArmsMesh)
	{
		FPSMagMesh->SetVisibility(true);
		FPSMagMesh->AttachToComponent(
			ArmsMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			ShellGrabSocketName
		);
		FPSMagMesh->SetRelativeTransform(FTransform::Identity);
		UE_LOG(LogPumpActionReload, Log, TEXT("FPS magazine shown and attached to Arms"));
	}

	// Show and attach TPS magazine mesh to Body mesh (visible to others)
	if (TPSMagMesh && BodyMesh)
	{
		TPSMagMesh->SetVisibility(true);
		TPSMagMesh->AttachToComponent(
			BodyMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			ShellGrabSocketName
		);
		TPSMagMesh->SetRelativeTransform(FTransform::Identity);
		UE_LOG(LogPumpActionReload, Log, TEXT("TPS magazine shown and attached to Body"));
	}
}

void UPumpActionReloadComponent::HideAndAttachMagazineToWeapon()
{
	AActor* OwnerItem = GetOwnerItem();
	if (!OwnerItem) return;
	if (!OwnerItem->Implements<UReloadableInterface>() || !OwnerItem->Implements<UHoldableInterface>()) return;

	// Get FPS/TPS magazine meshes from weapon via interface
	UPrimitiveComponent* FPSMagMesh = IReloadableInterface::Execute_GetFPSMagazineMesh(OwnerItem);
	UPrimitiveComponent* TPSMagMesh = IReloadableInterface::Execute_GetTPSMagazineMesh(OwnerItem);

	// Get weapon meshes for re-attachment
	UPrimitiveComponent* FPSWeaponMesh = IHoldableInterface::Execute_GetFPSMeshComponent(OwnerItem);
	UPrimitiveComponent* TPSWeaponMesh = IHoldableInterface::Execute_GetTPSMeshComponent(OwnerItem);

	UE_LOG(LogPumpActionReload, Log, TEXT("HideAndAttachMagazineToWeapon"));

	// Hide and re-attach FPS magazine mesh to weapon FPS mesh
	if (FPSMagMesh && FPSWeaponMesh)
	{
		FPSMagMesh->SetVisibility(false);
		FPSMagMesh->AttachToComponent(
			FPSWeaponMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			FName("magazine")  // Standard magazine socket on weapon
		);
		FPSMagMesh->SetRelativeTransform(FTransform::Identity);
		UE_LOG(LogPumpActionReload, Log, TEXT("FPS magazine hidden and attached to weapon"));
	}

	// Hide and re-attach TPS magazine mesh to weapon TPS mesh
	if (TPSMagMesh && TPSWeaponMesh)
	{
		TPSMagMesh->SetVisibility(false);
		TPSMagMesh->AttachToComponent(
			TPSWeaponMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			FName("magazine")  // Standard magazine socket on weapon
		);
		TPSMagMesh->SetRelativeTransform(FTransform::Identity);
		UE_LOG(LogPumpActionReload, Log, TEXT("TPS magazine hidden and attached to weapon"));
	}
}

void UPumpActionReloadComponent::SetMagazineVisibility(bool bVisible)
{
	AActor* OwnerItem = GetOwnerItem();
	if (!OwnerItem || !OwnerItem->Implements<UReloadableInterface>()) return;

	UPrimitiveComponent* FPSMagMesh = IReloadableInterface::Execute_GetFPSMagazineMesh(OwnerItem);
	UPrimitiveComponent* TPSMagMesh = IReloadableInterface::Execute_GetTPSMagazineMesh(OwnerItem);

	if (FPSMagMesh)
	{
		FPSMagMesh->SetVisibility(bVisible);
	}

	if (TPSMagMesh)
	{
		TPSMagMesh->SetVisibility(bVisible);
	}

	UE_LOG(LogPumpActionReload, Log, TEXT("SetMagazineVisibility: %s"), bVisible ? TEXT("true") : TEXT("false"));
}
