// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DamageEvents.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Enable replication for this component
	SetIsReplicatedByDefault(true);

	// ============================================
	// BONE DAMAGE MULTIPLIERS INITIALIZATION
	// ============================================
	// Initialize bone damage multipliers for hit location damage
	// Format: BoneName -> DamageMultiplier

	// Torso
	BoneDamageMultipliers.Add(FName("pelvis"), 0.4f);
	BoneDamageMultipliers.Add(FName("spine_02"), 0.375f);
	BoneDamageMultipliers.Add(FName("spine_03"), 0.4f);
	BoneDamageMultipliers.Add(FName("spine_04"), 0.425f);
	BoneDamageMultipliers.Add(FName("spine_05"), 0.45f);

	// Shoulders
	BoneDamageMultipliers.Add(FName("clavicle_l"), 0.6f);
	BoneDamageMultipliers.Add(FName("clavicle_r"), 0.6f);

	// Arms
	BoneDamageMultipliers.Add(FName("upperarm_l"), 0.5f);
	BoneDamageMultipliers.Add(FName("upperarm_r"), 0.5f);
	BoneDamageMultipliers.Add(FName("lowerarm_l"), 0.45f);
	BoneDamageMultipliers.Add(FName("lowerarm_r"), 0.45f);
	BoneDamageMultipliers.Add(FName("hand_l"), 0.3f);
	BoneDamageMultipliers.Add(FName("hand_r"), 0.3f);

	// ============================================
	// CRITICAL HIT ZONES (High multipliers for instant/critical kills)
	// ============================================
	// Head: x10.0 - Guaranteed instant kill (35 * 10 = 350+ damage)
	// Neck: x5.0 - Critical hit zone (35 * 5 = 175+ damage)
	BoneDamageMultipliers.Add(FName("neck_01"), 5.0f);
	BoneDamageMultipliers.Add(FName("neck_02"), 5.0f);
	BoneDamageMultipliers.Add(FName("head"), 10.0f);

	// Legs
	BoneDamageMultipliers.Add(FName("thigh_l"), 0.6f);
	BoneDamageMultipliers.Add(FName("thigh_r"), 0.6f);
	BoneDamageMultipliers.Add(FName("calf_l"), 0.55f);
	BoneDamageMultipliers.Add(FName("calf_r"), 0.55f);
	BoneDamageMultipliers.Add(FName("foot_l"), 0.3f);
	BoneDamageMultipliers.Add(FName("foot_r"), 0.3f);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize health to max
	Health = MaxHealth;
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate health and death state
	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, bIsDeath);
}

// ============================================
// DAMAGE SYSTEM
// ============================================

float UHealthComponent::ApplyDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Early exit if no damage or already dead
	if (DamageAmount <= 0.0f || bIsDeath)
	{
		return 0.0f;
	}

	// SERVER AUTHORITY: Damage logic runs on server only
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return 0.0f;
	}

	// ============================================
	// APPLY BONE DAMAGE MULTIPLIER (Point Damage only)
	// ============================================
	float ActualDamage = DamageAmount;
	FName HitBoneName = NAME_None;
	float BoneMultiplier = 1.0f;
	FVector HitLocation = FVector::ZeroVector;
	FVector HitDirection = FVector::ZeroVector;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		HitBoneName = PointDamageEvent->HitInfo.BoneName;
		HitLocation = PointDamageEvent->HitInfo.ImpactPoint;
		HitDirection = PointDamageEvent->ShotDirection;

		// Get bone multiplier and apply to damage
		BoneMultiplier = GetBoneDamageMultiplier(HitBoneName);
		ActualDamage *= BoneMultiplier;
	}

	// ============================================
	// UPDATE HEALTH STATE
	// ============================================
	Health -= ActualDamage;
	Health = FMath::Max(Health, 0.0f);

	// ============================================
	// DEBUG ON-SCREEN MESSAGE
	// ============================================
	FString InstigatorName = EventInstigator ? EventInstigator->GetName() : TEXT("nullptr");
	FString CauserName = DamageCauser ? DamageCauser->GetName() : TEXT("nullptr");

	// Base damage info
	FString DamageInfo = FString::Printf(TEXT("[DAMAGE] %s took %.1f damage\nHealth: %.1f / %.1f\nInstigator: %s\nCauser: %s"),
		*Owner->GetName(), ActualDamage, Health, MaxHealth, *InstigatorName, *CauserName);

	// Add damage type details
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		DamageInfo += FString::Printf(TEXT("\nType: Point Damage\nBone: %s (x%.2f multiplier)\nHit Location: %.1f, %.1f, %.1f\nDirection: %.2f, %.2f, %.2f"),
			*HitBoneName.ToString(),
			BoneMultiplier,
			HitLocation.X, HitLocation.Y, HitLocation.Z,
			HitDirection.X, HitDirection.Y, HitDirection.Z);
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		DamageInfo += TEXT("\nType: Radial Damage");
	}
	else
	{
		DamageInfo += TEXT("\nType: Generic Damage");
	}

	// Display on screen (5 second duration, red color)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, DamageInfo);
	}

	// Also log to console for reference
	UE_LOG(LogTemp, Warning, TEXT("%s"), *DamageInfo.Replace(TEXT("\n"), TEXT(" | ")));

	// ============================================
	// BROADCAST DELEGATES (Owner reacts)
	// ============================================

	// OnHealthChanged - for UI updates (SERVER only, CLIENTS via OnRep_Health)
	OnHealthChanged.Broadcast(Health);

	// OnDamaged - for hit reactions, visual effects (no params)
	OnDamaged.Broadcast();

	// ============================================
	// CHECK FOR DEATH
	// ============================================
	if (Health <= 0.0f)
	{
		bIsDeath = true;

		// OnDeath - for ragdoll, camera effects, respawn
		OnDeath.Broadcast();

		UE_LOG(LogTemp, Warning, TEXT("[HealthComponent] %s died!"), *Owner->GetName());
	}

	return ActualDamage;
}

float UHealthComponent::GetBoneDamageMultiplier(FName BoneName) const
{
	// Look up bone damage multiplier in map
	const float* Multiplier = BoneDamageMultipliers.Find(BoneName);

	// Return multiplier if found, otherwise return 1.0 (default - no modification)
	return Multiplier ? *Multiplier : 1.0f;
}

void UHealthComponent::ResetHealthState()
{
	// SERVER AUTHORITY: Only server can reset health state
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HealthComponent] ResetHealthState() called on non-authority! Ignored."));
		return;
	}

	// ============================================
	// RESET GAMEPLAY STATE
	// ============================================
	Health = MaxHealth;
	bIsDeath = false;

	// ============================================
	// BROADCAST DELEGATE (Owner reacts)
	// ============================================
	// Trigger OnHealthChanged on SERVER immediately
	// Replication will trigger OnRep_Health on CLIENTS
	OnHealthChanged.Broadcast(Health);

	UE_LOG(LogTemp, Log, TEXT("[HealthComponent] %s health state reset (Health: %.1f / %.1f, IsDeath: false)"),
		*Owner->GetName(), Health, MaxHealth);
}

// ============================================
// REPLICATION CALLBACKS
// ============================================

void UHealthComponent::OnRep_Health()
{
	// Called on CLIENTS when Health replicates from server
	// Broadcast OnHealthChanged delegate so owner can update UI

	OnHealthChanged.Broadcast(Health);

	UE_LOG(LogTemp, Log, TEXT("[HealthComponent] OnRep_Health: Broadcasting OnHealthChanged (%.1f)"), Health);
}

void UHealthComponent::OnRep_IsDeath()
{
	// Called on CLIENTS when bIsDeath replicates from server
	// Broadcast OnDeath delegate so owner can handle visual effects (ragdoll, camera, etc.)

	if (bIsDeath)
	{
		OnDeath.Broadcast();

		UE_LOG(LogTemp, Log, TEXT("[HealthComponent] OnRep_IsDeath: Broadcasting OnDeath delegate"));
	}
}
