// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ReloadComponent.h"
#include "BoxMagazineReloadComponent.generated.h"

/**
 * UBoxMagazineReloadComponent
 *
 * M4A1-style box magazine reload implementation.
 *
 * Reload Sequence:
 * 1. Animation starts (Body/Arms/Legs montages)
 * 2. AnimNotify "MagazineOut" fires → OnMagazineOut()
 *    - Detach magazine from weapon
 *    - Attach to character hand socket (weapon_r)
 * 3. AnimNotify "MagazineIn" fires → OnMagazineIn()
 *    - Detach magazine from hand
 *    - Re-attach to weapon socket (magazine)
 * 4. Montage completes → OnReloadComplete()
 *    - SERVER ONLY: Transfer ammo to magazine
 *    - Clear bIsReloading state
 *
 * Used by: Assault Rifles (M4A1), Pistols, SMGs, LMGs
 *
 * Future reload types:
 * - UBoltActionReloadComponent (single round loading)
 * - UTubeMagazineReloadComponent (shotgun tube reload)
 * - UBeltFedReloadComponent (machine gun belt reload)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPSCORE_API UBoxMagazineReloadComponent : public UReloadComponent
{
	GENERATED_BODY()

public:
	// ============================================
	// DESIGNER DEFAULTS - SOCKETS
	// ============================================

	// Character socket for magazine during reload (left hand grabs magazine)
	// Default: "weapon_l" (left hand socket)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Sockets")
	FName MagazineOutSocketName = "weapon_l";

	// Weapon socket for magazine when equipped
	// Default: "magazine" (magazine socket on weapon mesh)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "1 - Defaults|Sockets")
	FName MagazineInSocketName = "magazine";

	// ============================================
	// VIRTUAL OVERRIDES
	// ============================================

	/**
	 * Magazine detached from weapon → attach to character hand
	 * Called by UAnimNotify_MagazineOut during reload animation
	 *
	 * LOCAL operation (runs on all machines independently)
	 * - Gets FPS and TPS magazines from weapon
	 * - Attaches FPS magazine to character Arms socket
	 * - Attaches TPS magazine to character Body socket
	 * - Resets relative transform to Identity
	 */
	virtual void OnMagazineOut() override;

	/**
	 * Magazine re-attached to weapon → attach to weapon socket
	 * Called by UAnimNotify_MagazineIn during reload animation
	 *
	 * LOCAL operation (runs on all machines independently)
	 * - Gets FPS and TPS magazines from weapon
	 * - Re-attaches FPS magazine to weapon FPSMesh socket
	 * - Re-attaches TPS magazine to weapon TPSMesh socket
	 */
	virtual void OnMagazineIn() override;

	/**
	 * Reload complete → add ammo to magazine
	 * Called when reload montage completes successfully
	 *
	 * SERVER ONLY operation (HasAuthority check)
	 * - Calculates ammo needed (MaxCapacity - CurrentAmmo)
	 * - Adds ammo to magazine via IAmmoProviderInterface
	 * - Magazine->CurrentAmmo is REPLICATED (clients auto-update)
	 * - Clears bIsReloading state (replicates to clients)
	 */
	virtual void OnReloadComplete() override;

protected:
	/**
	 * Helper: Attach mesh component to socket
	 * Handles attachment with Identity transform (socket defines position)
	 *
	 * @param Mesh - Mesh component to attach (FPS or TPS magazine mesh)
	 * @param Parent - Parent component to attach to (Arms/Body/FPSMesh/TPSMesh)
	 * @param SocketName - Socket name on parent component
	 *
	 * NOTE: Mesh ALWAYS uses Identity relative transform.
	 *       Socket itself defines correct position/rotation.
	 */
	void AttachMeshToSocket(USceneComponent* Mesh, USceneComponent* Parent, FName SocketName);
};
