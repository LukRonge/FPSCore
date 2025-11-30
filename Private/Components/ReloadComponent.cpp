// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ReloadComponent.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/HoldableInterface.h"
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

bool UReloadComponent::CanReload_Internal() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;
	if (!OwnerActor->Implements<UAmmoConsumerInterface>()) return false;
	if (bIsReloading) return false;

	// Block reload during equip/unequip montages
	if (OwnerActor->Implements<UHoldableInterface>())
	{
		if (IHoldableInterface::Execute_IsEquipping(OwnerActor) ||
			IHoldableInterface::Execute_IsUnequipping(OwnerActor))
		{
			return false;
		}
	}

	int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(OwnerActor);
	int32 MaxAmmo = IAmmoConsumerInterface::Execute_GetClipSize(OwnerActor);

	return CurrentAmmo < MaxAmmo;
}

void UReloadComponent::Server_StartReload_Implementation(const FUseContext& Ctx)
{
	if (!CanReload_Internal()) return;

	bIsReloading = true;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PlayReloadMontages();
	}
}

void UReloadComponent::Server_CancelReload_Implementation()
{
	if (!bIsReloading) return;

	StopReloadMontages();
	bIsReloading = false;
}

void UReloadComponent::OnRep_IsReloading()
{
	if (bIsReloading)
	{
		PlayReloadMontages();
	}
	else
	{
		StopReloadMontages();
	}
}

void UReloadComponent::PlayReloadMontages()
{
	AActor* CharacterActor = GetOwnerCharacterActor();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;
	if (!ReloadMontage) return;

	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
	USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

	if (BodyMesh)
	{
		UAnimInstance* BodyAnimInst = BodyMesh->GetAnimInstance();
		if (BodyAnimInst)
		{
			BodyAnimInst->Montage_Play(ReloadMontage);

			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &UReloadComponent::OnMontageEnded);
			BodyAnimInst->Montage_SetEndDelegate(EndDelegate, ReloadMontage);
		}
	}

	if (ArmsMesh)
	{
		UAnimInstance* ArmsAnimInst = ArmsMesh->GetAnimInstance();
		if (ArmsAnimInst)
		{
			ArmsAnimInst->Montage_Play(ReloadMontage);
		}
	}

	if (LegsMesh)
	{
		UAnimInstance* LegsAnimInst = LegsMesh->GetAnimInstance();
		if (LegsAnimInst)
		{
			LegsAnimInst->Montage_Play(ReloadMontage);
		}
	}
}

void UReloadComponent::StopReloadMontages()
{
	AActor* CharacterActor = GetOwnerCharacterActor();
	if (!CharacterActor || !CharacterActor->Implements<UCharacterMeshProviderInterface>() || !ReloadMontage) return;

	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);
	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
	USkeletalMeshComponent* LegsMesh = ICharacterMeshProviderInterface::Execute_GetLegsMesh(CharacterActor);

	if (BodyMesh)
	{
		UAnimInstance* BodyAnimInst = BodyMesh->GetAnimInstance();
		if (BodyAnimInst && BodyAnimInst->Montage_IsPlaying(ReloadMontage))
		{
			BodyAnimInst->Montage_Stop(0.2f, ReloadMontage);
		}
	}

	if (ArmsMesh)
	{
		UAnimInstance* ArmsAnimInst = ArmsMesh->GetAnimInstance();
		if (ArmsAnimInst && ArmsAnimInst->Montage_IsPlaying(ReloadMontage))
		{
			ArmsAnimInst->Montage_Stop(0.2f, ReloadMontage);
		}
	}

	if (LegsMesh)
	{
		UAnimInstance* LegsAnimInst = LegsMesh->GetAnimInstance();
		if (LegsAnimInst && LegsAnimInst->Montage_IsPlaying(ReloadMontage))
		{
			LegsAnimInst->Montage_Stop(0.2f, ReloadMontage);
		}
	}
}

void UReloadComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	if (bInterrupted)
	{
		bIsReloading = false;
		return;
	}

	OnReloadComplete();
}

void UReloadComponent::OnReloadComplete()
{
	bIsReloading = false;
}

void UReloadComponent::OnMagazineOut()
{
}

void UReloadComponent::OnMagazineIn()
{
}

AActor* UReloadComponent::GetOwnerCharacterActor() const
{
	AActor* OwnerItem = GetOwner();
	if (!OwnerItem) return nullptr;
	return OwnerItem->GetOwner();
}
