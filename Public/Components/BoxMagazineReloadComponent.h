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
	// CONFIGURATION
	// ============================================

	/**
	 * Character bone for magazine attachment during reload
	 * Default: "weapon_r" (right hand socket)
	 * Magazine attaches here when detached from weapon
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Reload|Magazine")
	FName MagazineOutSocketName = "weapon_r";

	/**
	 * Weapon bone for magazine attachment when equipped
	 * Default: "magazine" (magazine socket on weapon mesh)
	 * Magazine re-attaches here when reload completes
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Reload|Magazine")
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
	 * - Adds ammo to both FPS and TPS magazines
	 * - Magazine->CurrentAmmo is REPLICATED (clients auto-update)
	 * - Clears bIsReloading state (replicates to clients)
	 *
	 * NOTE: FPS and TPS magazines are separate actors with separate
	 * CurrentAmmo properties. Both must be updated for visual sync.
	 */
	virtual void OnReloadComplete() override;

protected:
	/**
	 * Helper: Attach magazine actor to socket
	 * Handles attachment and transform reset for magazine repositioning
	 *
	 * @param Magazine - Magazine actor to attach
	 * @param Parent - Parent component to attach to (Arms/Body/FPSMesh/TPSMesh)
	 * @param SocketName - Socket name on parent component
	 */
	void AttachMagazineToSocket(AActor* Magazine, USceneComponent* Parent, FName SocketName);
};
