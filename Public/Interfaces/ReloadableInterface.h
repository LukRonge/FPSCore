// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "ReloadableInterface.generated.h"

class UPrimitiveComponent;

/**
 * CAPABILITY: Reloadable
 * Interface for items that can be reloaded (weapons)
 *
 * Implemented by: Firearms
 * Design: Query-Command (CanReload + Reload)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UReloadableInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSCORE_API IReloadableInterface
{
	GENERATED_BODY()

public:
	// Check if reload is possible right now
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable")
	bool CanReload() const;
	virtual bool CanReload_Implementation() const { return true; }

	// Start reload sequence (SERVER ONLY)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable")
	void Reload(const FUseContext& Ctx);
	virtual void Reload_Implementation(const FUseContext& Ctx) { }

	// Check if currently reloading
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable")
	bool IsReloading() const;
	virtual bool IsReloading_Implementation() const { return false; }

	// ============================================
	// MAGAZINE ACCESS (for AnimNotifies and ReloadComponent subclasses)
	// ============================================

	/**
	 * Get magazine actor (single authoritative magazine)
	 * @return Magazine actor or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Magazine")
	AActor* GetMagazineActor() const;
	virtual AActor* GetMagazineActor_Implementation() const { return nullptr; }

	/**
	 * Get FPS magazine mesh (visible only to owner)
	 * Delegates to magazine actor via IMagazineMeshProviderInterface
	 * @return FPS mesh component or nullptr
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Magazine")
	UPrimitiveComponent* GetFPSMagazineMesh() const;
	virtual UPrimitiveComponent* GetFPSMagazineMesh_Implementation() const { return nullptr; }

	/**
	 * Get TPS magazine mesh (visible to others, not owner)
	 * Delegates to magazine actor via IMagazineMeshProviderInterface
	 * @return TPS mesh component or nullptr
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Magazine")
	UPrimitiveComponent* GetTPSMagazineMesh() const;
	virtual UPrimitiveComponent* GetTPSMagazineMesh_Implementation() const { return nullptr; }

	/**
	 * Get ReloadComponent from this reloadable item
	 * Used by AnimNotifies to trigger OnMagazineOut/OnMagazineIn callbacks
	 * @return ReloadComponent or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Component")
	class UReloadComponent* GetReloadComponent() const;
	virtual class UReloadComponent* GetReloadComponent_Implementation() const { return nullptr; }

	/**
	 * Called when reload completes successfully
	 * Allows weapon-specific behavior after reload (e.g., M4A1 bolt carrier release)
	 * Called from ReloadComponent::OnReloadComplete()
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Callback")
	void OnWeaponReloadComplete();
	virtual void OnWeaponReloadComplete_Implementation() { }
};
