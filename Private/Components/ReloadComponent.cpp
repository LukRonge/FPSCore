// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ReloadComponent.h"
#include "BaseWeapon.h"
#include "FPSCharacter.h"
#include "BaseMagazine.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

UReloadComponent::UReloadComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UReloadComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UReloadComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UReloadComponent, bIsReloading);
}

// ============================================
// PUBLIC API
// ============================================

bool UReloadComponent::CanReload_Internal() const
{
	// ✅ CORRECT: Use GetOwner() - no direct cast
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ CanReload_Internal - No owner actor"));
		return false;
	}

	// ✅ CORRECT: Check if owner implements IAmmoConsumerInterface (interface check, no cast)
	if (!OwnerActor->Implements<UAmmoConsumerInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ CanReload_Internal - Owner does not implement IAmmoConsumerInterface"));
		return false;
	}

	// Check if already reloading
	if (bIsReloading)
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ CanReload_Internal - Already reloading"));
		return false;
	}

	// ✅ CORRECT: Get ammo info via IAmmoConsumerInterface (no direct Magazine access)
	int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(OwnerActor);
	int32 MaxAmmo = IAmmoConsumerInterface::Execute_GetClipSize(OwnerActor);

	// Check if magazine needs reload
	if (CurrentAmmo >= MaxAmmo)
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ CanReload_Internal - Magazine is full (CurrentAmmo: %d / MaxAmmo: %d)"), CurrentAmmo, MaxAmmo);
		return false;
	}

	// TODO: Check reserve ammo availability via IAmmoProviderInterface (future feature)
	// if (GetTotalAmmo() <= 0) return false;

	UE_LOG(LogTemp, Log, TEXT("✅ CanReload_Internal - All checks passed! Magazine: %d/%d"), CurrentAmmo, MaxAmmo);
	return true;
}

void UReloadComponent::Server_StartReload_Implementation(const FUseContext& Ctx)
{
	// Validate on server
	if (!CanReload_Internal())
	{
		UE_LOG(LogTemp, Warning, TEXT("ReloadComponent: Server_StartReload - CanReload check failed"));
		return;
	}

	// Set replicated state
	bIsReloading = true;

	// Server executes immediately (HasAuthority check)
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PlayReloadMontages();
		UE_LOG(LogTemp, Log, TEXT("ReloadComponent: Server_StartReload - Reload started on server"));
	}
}

void UReloadComponent::Server_CancelReload_Implementation()
{
	if (!bIsReloading) return;

	// Stop montages
	StopReloadMontages();

	// Clear state
	bIsReloading = false;

	UE_LOG(LogTemp, Log, TEXT("ReloadComponent: Reload cancelled"));
}

// ============================================
// REPLICATION CALLBACKS
// ============================================

void UReloadComponent::OnRep_IsReloading()
{
	// Clients execute when replicated (OnRep runs ONLY on clients)
	if (bIsReloading)
	{
		PlayReloadMontages();
		UE_LOG(LogTemp, Log, TEXT("ReloadComponent: OnRep_IsReloading - Starting reload montages on client"));
	}
	else
	{
		StopReloadMontages();
		UE_LOG(LogTemp, Log, TEXT("ReloadComponent: OnRep_IsReloading - Stopping reload montages on client"));
	}
}

// ============================================
// VIRTUAL INTERFACE (Override in Subclasses)
// ============================================

void UReloadComponent::PlayReloadMontages()
{
	AFPSCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReloadComponent: PlayReloadMontages - No character owner"));
		return;
	}

	if (!ReloadMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReloadComponent: PlayReloadMontages - ReloadMontage is nullptr"));
		return;
	}

	// Play on Body mesh + BIND COMPLETION DELEGATE
	UAnimInstance* BodyAnimInst = Character->GetMesh()->GetAnimInstance();
	if (BodyAnimInst)
	{
		BodyAnimInst->Montage_Play(ReloadMontage);

		// CRITICAL: Bind OnMontageEnded delegate
		// Without this, OnReloadComplete() will NEVER be called!
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UReloadComponent::OnMontageEnded);
		BodyAnimInst->Montage_SetEndDelegate(EndDelegate, ReloadMontage);

		UE_LOG(LogTemp, Log, TEXT("ReloadComponent: Playing Body montage with delegate binding"));
	}

	// Play on Arms mesh (same montage, no delegate needed)
	if (Character->Arms)
	{
		UAnimInstance* ArmsAnimInst = Character->Arms->GetAnimInstance();
		if (ArmsAnimInst)
		{
			ArmsAnimInst->Montage_Play(ReloadMontage);
			UE_LOG(LogTemp, Log, TEXT("ReloadComponent: Playing Arms montage"));
		}
	}

	// Play on Legs mesh (same montage, if exists)
	if (Character->Legs)
	{
		UAnimInstance* LegsAnimInst = Character->Legs->GetAnimInstance();
		if (LegsAnimInst)
		{
			LegsAnimInst->Montage_Play(ReloadMontage);
			UE_LOG(LogTemp, Log, TEXT("ReloadComponent: Playing Legs montage"));
		}
	}
}

void UReloadComponent::StopReloadMontages()
{
	AFPSCharacter* Character = GetOwnerCharacter();
	if (!Character || !ReloadMontage) return;

	// Stop Body montage
	UAnimInstance* BodyAnimInst = Character->GetMesh()->GetAnimInstance();
	if (BodyAnimInst && BodyAnimInst->Montage_IsPlaying(ReloadMontage))
	{
		BodyAnimInst->Montage_Stop(0.2f, ReloadMontage);
	}

	// Stop Arms montage
	if (Character->Arms)
	{
		UAnimInstance* ArmsAnimInst = Character->Arms->GetAnimInstance();
		if (ArmsAnimInst && ArmsAnimInst->Montage_IsPlaying(ReloadMontage))
		{
			ArmsAnimInst->Montage_Stop(0.2f, ReloadMontage);
		}
	}

	// Stop Legs montage
	if (Character->Legs)
	{
		UAnimInstance* LegsAnimInst = Character->Legs->GetAnimInstance();
		if (LegsAnimInst && LegsAnimInst->Montage_IsPlaying(ReloadMontage))
		{
			LegsAnimInst->Montage_Stop(0.2f, ReloadMontage);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ReloadComponent: Reload montages stopped"));
}

void UReloadComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Called on SERVER + CLIENTS when montage ends
	UE_LOG(LogTemp, Log, TEXT("ReloadComponent: OnMontageEnded - Interrupted: %s, Authority: %s"),
		bInterrupted ? TEXT("YES") : TEXT("NO"),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"));

	// Only server processes completion
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// If interrupted, just clear state
	if (bInterrupted)
	{
		bIsReloading = false;
		UE_LOG(LogTemp, Warning, TEXT("ReloadComponent: Reload interrupted"));
		return;
	}

	// Normal completion - call OnReloadComplete (SERVER ONLY)
	OnReloadComplete();
}

void UReloadComponent::OnReloadComplete()
{
	// Base implementation does nothing
	// MUST BE OVERRIDDEN in subclasses to implement ammo transfer
	UE_LOG(LogTemp, Warning, TEXT("ReloadComponent: OnReloadComplete called on base class - should be overridden!"));

	// Clear reload state (will replicate to clients)
	bIsReloading = false;
}

// ============================================
// ANIMNOTIFY CALLBACKS
// ============================================

void UReloadComponent::OnMagazineOut()
{
	// Base implementation does nothing
	// MUST BE OVERRIDDEN in subclasses
	UE_LOG(LogTemp, Log, TEXT("ReloadComponent: OnMagazineOut called (base implementation)"));
}

void UReloadComponent::OnMagazineIn()
{
	// Base implementation does nothing
	// MUST BE OVERRIDDEN in subclasses
	UE_LOG(LogTemp, Log, TEXT("ReloadComponent: OnMagazineIn called (base implementation)"));
}

// ============================================
// HELPER FUNCTIONS
// ============================================

ABaseWeapon* UReloadComponent::GetOwnerWeapon() const
{
	return Cast<ABaseWeapon>(GetOwner());
}

AFPSCharacter* UReloadComponent::GetOwnerCharacter() const
{
	ABaseWeapon* Weapon = GetOwnerWeapon();
	if (!Weapon) return nullptr;

	return Cast<AFPSCharacter>(Weapon->GetOwner());
}
