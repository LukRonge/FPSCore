// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/RecoilComponent.h"
#include "FPSCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"

URecoilComponent::URecoilComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Disabled by default, enabled when recoil active

	// NOT replicated - local state tracking on each machine
	SetIsReplicatedByDefault(false);
}

void URecoilComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize state
	CurrentRecoilPitch = 0.0f;
	CurrentRecoilYaw = 0.0f;
	ShotCount = 0;
	LastShotTime = 0.0f;
}

void URecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Early out if no active recoil
	if (FMath::Abs(CurrentRecoilPitch) < KINDA_SMALL_NUMBER &&
		FMath::Abs(CurrentRecoilYaw) < KINDA_SMALL_NUMBER &&
		ShotCount == 0)
	{
		// Disable tick when fully recovered
		SetComponentTickEnabled(false);
		return;
	}

	// Check if recovery delay has passed
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float TimeSinceLastShot = CurrentTime - LastShotTime;

	if (TimeSinceLastShot > RecoveryDelay)
	{
		// Recovery interpolation (smooth return to center)
		CurrentRecoilPitch = FMath::FInterpTo(
			CurrentRecoilPitch,
			0.0f,
			DeltaTime,
			RecoverySpeed
		);

		CurrentRecoilYaw = FMath::FInterpTo(
			CurrentRecoilYaw,
			0.0f,
			DeltaTime,
			RecoverySpeed
		);

		// Decay shot count (exponential decay)
		// Decay rate: 5 shots per second
		float DecayRate = 5.0f;
		int32 DecayAmount = FMath::CeilToInt(DeltaTime * DecayRate);
		ShotCount = FMath::Max(0, ShotCount - DecayAmount);
	}
}

// ============================================
// RECOIL APPLICATION
// ============================================

void URecoilComponent::AddRecoil(float Scale)
{
	// SERVER ONLY - state tracking for accumulation
	// This does NOT apply visual feedback (that happens in Multicast RPC)

	// Update shot count
	ShotCount++;

	// Update last shot time (for recovery delay)
	LastShotTime = GetWorld()->GetTimeSeconds();

	// Enable tick for recovery interpolation
	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Verbose, TEXT("RecoilComponent::AddRecoil() - Scale=%.2f, ShotCount=%d"), Scale, ShotCount);
}

void URecoilComponent::ApplyRecoilToCamera(float Scale)
{
	// OWNING CLIENT ONLY - camera kick
	AFPSCharacter* Character = Cast<AFPSCharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("RecoilComponent::ApplyRecoilToCamera() - Owner is not AFPSCharacter!"));
		return;
	}

	if (!Character->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("RecoilComponent::ApplyRecoilToCamera() - Called on non-locally controlled character!"));
		return;
	}

	// Get ADS modifier from character aiming state
	float ADSModifier = 1.0f;
	if (Character->bIsAiming)
	{
		ADSModifier = ADSRecoilMultiplier; // 0.5 = 50% recoil when aiming
	}

	// Get accumulation multiplier (1.0x → 2.0x based on shot count)
	float AccumulationMultiplier = GetAccumulationMultiplier();

	// Calculate vertical kick (upward pitch)
	float VerticalKick = BaseVerticalRecoil * Scale * AccumulationMultiplier * ADSModifier;

	// Calculate horizontal kick (random ± with bias)
	float HorizontalKick = FMath::RandRange(-BaseHorizontalRecoil, BaseHorizontalRecoil);
	HorizontalKick = (HorizontalKick + HorizontalBias) * Scale * ADSModifier;

	// Apply to camera
	// Negative pitch = kick upward (camera looks up)
	Character->UpdatePitch(-VerticalKick);
	Character->AddControllerYawInput(HorizontalKick);

	// Track accumulated recoil for recovery
	CurrentRecoilPitch += VerticalKick;
	CurrentRecoilYaw += HorizontalKick;

	// Update state
	ShotCount++;
	LastShotTime = GetWorld()->GetTimeSeconds();

	// Enable tick for recovery
	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Verbose, TEXT("RecoilComponent::ApplyRecoilToCamera() - Vertical=%.2f, Horizontal=%.2f, Accumulation=%.2f, ADS=%.2f"),
		VerticalKick, HorizontalKick, AccumulationMultiplier, ADSModifier);
}

void URecoilComponent::ApplyRecoilToWeapon(float Scale)
{
	// REMOTE CLIENTS ONLY - weapon animation
	AFPSCharacter* Character = Cast<AFPSCharacter>(GetOwner());
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("RecoilComponent::ApplyRecoilToWeapon() - Owner is not AFPSCharacter!"));
		return;
	}

	if (Character->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("RecoilComponent::ApplyRecoilToWeapon() - Called on locally controlled character!"));
		return;
	}

	// TODO: Implement weapon TPS mesh animation
	// Options:
	// 1. Play weapon recoil animation montage on Body mesh
	// 2. Add procedural rotation offset to weapon TPS mesh
	// 3. Use timeline for smooth kick/return animation

	// For now, just track state
	ShotCount++;
	LastShotTime = GetWorld()->GetTimeSeconds();

	// Enable tick for state decay
	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Verbose, TEXT("RecoilComponent::ApplyRecoilToWeapon() - Scale=%.2f (TODO: Implement weapon animation)"), Scale);
}

void URecoilComponent::ResetRecoil()
{
	// Clear all recoil state
	CurrentRecoilPitch = 0.0f;
	CurrentRecoilYaw = 0.0f;
	ShotCount = 0;
	LastShotTime = 0.0f;

	// Disable tick
	SetComponentTickEnabled(false);

	UE_LOG(LogTemp, Log, TEXT("RecoilComponent::ResetRecoil() - Recoil state cleared"));
}

// ============================================
// HELPERS
// ============================================

float URecoilComponent::GetAccumulationMultiplier() const
{
	// Calculate accumulation factor (0.0 → 1.0 based on shot count)
	float AccumulationFactor = FMath::Min(static_cast<float>(ShotCount) / MaxAccumulation, 1.0f);

	// Apply accumulation curve (1.0x → 1.4x) - REDUCED for controllable recoil
	// First shot: 1.0x (base recoil)
	// MaxAccumulation shots: 1.4x (40% increase, not 100%)
	// This allows player to compensate by pulling mouse down steadily
	float Multiplier = 1.0f + (AccumulationFactor * 0.4f);

	return Multiplier;
}
