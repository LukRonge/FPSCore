// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/BoxMagazineReloadComponent.h"
#include "Interfaces/ReloadableInterface.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/CharacterMeshProviderInterface.h"
#include "Interfaces/AmmoConsumerInterface.h"
#include "Interfaces/AmmoProviderInterface.h"
#include "Components/SkeletalMeshComponent.h"

void UBoxMagazineReloadComponent::OnMagazineOut()
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

	// Attach FPS magazine mesh to Arms mesh (visible only to owner)
	if (FPSMagMesh && ArmsMesh)
	{
		AttachMeshToSocket(FPSMagMesh, ArmsMesh, MagazineOutSocketName);
	}

	// Attach TPS magazine mesh to Body mesh (visible to others)
	if (TPSMagMesh && BodyMesh)
	{
		AttachMeshToSocket(TPSMagMesh, BodyMesh, MagazineOutSocketName);
	}
}

void UBoxMagazineReloadComponent::OnMagazineIn()
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

	// Re-attach FPS magazine mesh to weapon FPS mesh
	if (FPSMagMesh && FPSWeaponMesh)
	{
		AttachMeshToSocket(FPSMagMesh, FPSWeaponMesh, MagazineInSocketName);
	}

	// Re-attach TPS magazine mesh to weapon TPS mesh
	if (TPSMagMesh && TPSWeaponMesh)
	{
		AttachMeshToSocket(TPSMagMesh, TPSWeaponMesh, MagazineInSocketName);
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
		// Get single magazine actor and add ammo
		AActor* MagActor = IReloadableInterface::Execute_GetMagazineActor(OwnerItem);
		if (MagActor && MagActor->Implements<UAmmoProviderInterface>())
		{
			IAmmoProviderInterface::Execute_AddAmmoToProvider(MagActor, AmmoNeeded);
		}

		// Notify weapon that reload completed (for weapon-specific behavior like M4A1 bolt release)
		IReloadableInterface::Execute_OnWeaponReloadComplete(OwnerItem);
	}

	bIsReloading = false;
}

void UBoxMagazineReloadComponent::AttachMeshToSocket(USceneComponent* Mesh, USceneComponent* Parent, FName SocketName)
{
	if (!Mesh || !Parent) return;

	Mesh->AttachToComponent(
		Parent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);

	Mesh->SetRelativeTransform(FTransform::Identity);
}
