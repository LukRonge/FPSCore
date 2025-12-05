// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSGameMode.h"
#include "GameFramework/PlayerStart.h"
#include "Interfaces/DamageableInterface.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

AFPSGameMode::AFPSGameMode()
{
}

void AFPSGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AFPSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto& Pair : PendingRespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	PendingRespawnTimers.Empty();

	Super::EndPlay(EndPlayReason);
}

void AFPSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	PlayerControllers.Add(NewPlayer);
	PlayerIDs++;
	SpawnPlayerCharacter(NewPlayer);
}

void AFPSGameMode::SpawnPlayerCharacter(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	APawn* OldPawn = PC->GetPawn();
	if (OldPawn)
	{
		PC->UnPossess();
		OldPawn->Destroy();
	}

	AActor* PlayerStart = FindPlayerStart(PC);
	if (!PlayerStart)
	{
		return;
	}

	if (!DefaultPawnClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(
		DefaultPawnClass,
		PlayerStart->GetActorLocation(),
		PlayerStart->GetActorRotation(),
		SpawnParams
	);

	if (NewPawn)
	{
		PC->Possess(NewPawn);

		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

// ============================================
// GAME MODE DEATH INTERFACE IMPLEMENTATION
// ============================================

void AFPSGameMode::OnPlayerDeath_Implementation(AController* PlayerController, APawn* DeadPawn, AActor* Killer)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	if (FTimerHandle* ExistingTimer = PendingRespawnTimers.Find(PlayerController))
	{
		GetWorldTimerManager().ClearTimer(*ExistingTimer);
	}

	if (RespawnDelay > 0.0f)
	{
		FTimerHandle& RespawnTimer = PendingRespawnTimers.FindOrAdd(PlayerController);
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUFunction(this, FName("RespawnPlayer"), PlayerController);
		GetWorldTimerManager().SetTimer(RespawnTimer, TimerDelegate, RespawnDelay, false);
	}
	else
	{
		RespawnPlayer(PlayerController);
	}
}

void AFPSGameMode::RespawnPlayer(AController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	PendingRespawnTimers.Remove(PlayerController);

	APawn* Pawn = PlayerController->GetPawn();
	if (!Pawn)
	{
		return;
	}

	FVector SpawnLocation;
	if (FindRespawnLocation(SpawnLocation))
	{
		Pawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (Pawn->Implements<UDamageableInterface>())
	{
		IDamageableInterface::Execute_ResetAfterDeath(Pawn);
	}
}

bool AFPSGameMode::FindRespawnLocation(FVector& OutLocation)
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; ++Attempt)
	{
		FVector RandomPoint = UKismetMathLibrary::RandomPointInBoundingBox(SpawnAreaCenter, SpawnAreaHalfExtents);

		FVector TraceStart = RandomPoint + FVector(0.0f, 0.0f, SpawnTraceHeight);
		FVector TraceEnd = RandomPoint;

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = false;

		bool bHit = World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams
		);

		if (bHit)
		{
			OutLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, SpawnGroundOffset);
			return true;
		}
	}

	return false;
}
