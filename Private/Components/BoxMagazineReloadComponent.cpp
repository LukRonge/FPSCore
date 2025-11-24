// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoxMagazineReloadComponent.h"
#include "Interfaces/ReloadableInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "BaseMagazine.h"
#include "Components/SkeletalMeshComponent.h"

void UBoxMagazineReloadComponent::OnMagazineOut()
{
	AActor* OwnerItem = GetOwnerItem();
	AActor* CharacterActor = GetOwnerCharacterActor();

	if (!OwnerItem || !CharacterActor) return;
	if (!OwnerItem->Implements<UReloadableInterface>()) return;
	if (!CharacterActor->Implements<UCharacterMeshProviderInterface>()) return;

	AActor* FPSMag = IReloadableInterface::Execute_GetFPSMagazineActor(OwnerItem);
	AActor* TPSMag = IReloadableInterface::Execute_GetTPSMagazineActor(OwnerItem);

	if (!FPSMag || !TPSMag) return;

	USkeletalMeshComponent* ArmsMesh = ICharacterMeshProviderInterface::Execute_GetArmsMesh(CharacterActor);
	USkeletalMeshComponent* BodyMesh = ICharacterMeshProviderInterface::Execute_GetBodyMesh(CharacterActor);

	if (ArmsMesh)
	{
		AttachMagazineToSocket(FPSMag, ArmsMesh, MagazineOutSocketName);
	}

	if (BodyMesh)
	{
		AttachMagazineToSocket(TPSMag, BodyMesh, MagazineOutSocketName);
	}
}

void UBoxMagazineReloadComponent::OnMagazineIn()
{
	AActor* OwnerItem = GetOwnerItem();
	if (!OwnerItem) return;
	if (!OwnerItem->Implements<UReloadableInterface>() || !OwnerItem->Implements<UHoldableInterface>()) return;

	AActor* FPSMag = IReloadableInterface::Execute_GetFPSMagazineActor(OwnerItem);
	AActor* TPSMag = IReloadableInterface::Execute_GetTPSMagazineActor(OwnerItem);

	if (!FPSMag || !TPSMag) return;

	UPrimitiveComponent* FPSMeshPrim = IHoldableInterface::Execute_GetFPSMeshComponent(OwnerItem);
	UPrimitiveComponent* TPSMeshPrim = IHoldableInterface::Execute_GetTPSMeshComponent(OwnerItem);

	if (FPSMeshPrim)
	{
		AttachMagazineToSocket(FPSMag, FPSMeshPrim, MagazineInSocketName);
	}

	if (TPSMeshPrim)
	{
		AttachMagazineToSocket(TPSMag, TPSMeshPrim, MagazineInSocketName);
	}
}

void UBoxMagazineReloadComponent::OnReloadComplete()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	AActor* OwnerItem = GetOwnerItem();
	if (!OwnerItem) return;
	if (!OwnerItem->Implements<UAmmoConsumerInterface>()) return;

	int32 CurrentAmmo = IAmmoConsumerInterface::Execute_GetClip(OwnerItem);
	int32 MaxCapacity = IAmmoConsumerInterface::Execute_GetClipSize(OwnerItem);
	int32 AmmoNeeded = MaxCapacity - CurrentAmmo;

	if (AmmoNeeded <= 0)
	{
		bIsReloading = false;
		return;
	}

	if (OwnerItem->Implements<UReloadableInterface>())
	{
		AActor* TPSMagActor = IReloadableInterface::Execute_GetTPSMagazineActor(OwnerItem);
		if (ABaseMagazine* CurrentMag = Cast<ABaseMagazine>(TPSMagActor))
		{
			CurrentMag->AddAmmo(AmmoNeeded);

			AActor* FPSMagActor = IReloadableInterface::Execute_GetFPSMagazineActor(OwnerItem);
			if (ABaseMagazine* FPSMag = Cast<ABaseMagazine>(FPSMagActor))
			{
				FPSMag->CurrentAmmo = CurrentMag->CurrentAmmo;
			}
		}
	}

	bIsReloading = false;
}

void UBoxMagazineReloadComponent::AttachMagazineToSocket(AActor* Magazine, USceneComponent* Parent, FName SocketName)
{
	if (!Magazine || !Parent) return;

	USceneComponent* MagRoot = Magazine->GetRootComponent();
	if (!MagRoot) return;

	MagRoot->AttachToComponent(
		Parent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);

	MagRoot->SetRelativeTransform(FTransform::Identity);
}
