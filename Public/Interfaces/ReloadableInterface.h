// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/InteractionContext.h"
#include "ReloadableInterface.generated.h"

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
	 * Get FPS magazine actor (visual representation for first-person)
	 * @return FPS magazine actor or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Magazine")
	AActor* GetFPSMagazineActor() const;
	virtual AActor* GetFPSMagazineActor_Implementation() const { return nullptr; }

	/**
	 * Get TPS magazine actor (visual representation for third-person)
	 * @return TPS magazine actor or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reloadable|Magazine")
	AActor* GetTPSMagazineActor() const;
	virtual AActor* GetTPSMagazineActor_Implementation() const { return nullptr; }

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
