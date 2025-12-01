// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/GrenadeProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"

AGrenadeProjectile::AGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replication setup - projectile must replicate to all clients
	bReplicates = true;
	bAlwaysRelevant = true;

	// Create collision component (root)
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(5.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComponent->SetSimulatePhysics(false); // Controlled by ProjectileMovement
	RootComponent = CollisionComponent;

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(true);

	// Create projectile movement component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 2000.0f;
	ProjectileMovement->MaxSpeed = 10000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = Bounciness;
	ProjectileMovement->Friction = Friction;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	// Don't auto-activate - we'll set velocity in InitializeThrow
	ProjectileMovement->bAutoActivate = true;
}

void AGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGrenadeProjectile, bHasExploded);
}

void AGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Apply physics settings from config
	if (ProjectileMovement)
	{
		ProjectileMovement->Bounciness = Bounciness;
		ProjectileMovement->Friction = Friction;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
	}
}

void AGrenadeProjectile::OnRep_HasExploded()
{
	// For late-joiners: if grenade already exploded, hide the mesh
	if (bHasExploded)
	{
		if (MeshComponent)
		{
			MeshComponent->SetVisibility(false);
		}
	}
}

void AGrenadeProjectile::InitializeProjectile_Implementation(APawn* InInstigator)
{
	// Store instigator for damage attribution
	InstigatorPawn = InInstigator;

	// ProjectileMovementComponent automatically uses InitialSpeed and actor's forward direction

	// Start fuse timer (SERVER ONLY)
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			FuseTimerHandle,
			this,
			&AGrenadeProjectile::OnFuseExpired,
			FuseTime,
			false // No loop
		);

		UE_LOG(LogTemp, Log, TEXT("AGrenadeProjectile::InitializeProjectile - Fuse started, %.1fs until explosion"), FuseTime);
	}
}

void AGrenadeProjectile::OnFuseExpired()
{
	// SERVER ONLY
	if (!HasAuthority())
	{
		return;
	}

	// Prevent double explosion
	if (bHasExploded)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AGrenadeProjectile::OnFuseExpired - Exploding at %s"), *GetActorLocation().ToString());

	// Apply damage (server authoritative)
	ApplyExplosionDamage();

	// Set replicated state - triggers OnRep on clients
	bHasExploded = true;

	// Play effects on all clients
	Multicast_PlayExplosionEffects();

	// Start destroy timer
	StartDestroyTimer();
}

void AGrenadeProjectile::ApplyExplosionDamage()
{
	// SERVER ONLY
	if (!HasAuthority())
	{
		return;
	}

	// Get damage origin
	FVector ExplosionOrigin = GetActorLocation();

	// Get instigator controller for damage attribution
	AController* InstigatorController = nullptr;
	if (InstigatorPawn)
	{
		InstigatorController = InstigatorPawn->GetController();
	}

	// Calculate minimum damage
	float MinDamage = ExplosionDamage * MinDamageMultiplier;

	// Apply radial damage with falloff
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		GetWorld(),
		ExplosionDamage,           // BaseDamage
		MinDamage,                 // MinimumDamage
		ExplosionOrigin,           // Origin
		ExplosionInnerRadius,      // DamageInnerRadius (full damage)
		ExplosionRadius,           // DamageOuterRadius (falloff to min)
		DamageFalloff,             // DamageFalloff exponent
		DamageTypeClass,           // DamageTypeClass
		TArray<AActor*>(),         // IgnoreActors (none)
		this,                      // DamageCauser
		InstigatorController,      // InstigatorController
		ECollisionChannel::ECC_Visibility  // DamagePreventionChannel
	);

	UE_LOG(LogTemp, Log, TEXT("AGrenadeProjectile::ApplyExplosionDamage - Damage: %.1f, Radius: %.1f"), ExplosionDamage, ExplosionRadius);
}

void AGrenadeProjectile::Multicast_PlayExplosionEffects_Implementation()
{
	// Play VFX
	if (ExplosionVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ExplosionVFX,
			GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(1.0f),       // Scale
			true,                // bAutoDestroy
			true,                // bAutoActivate
			ENCPoolMethod::None, // Pooling
			true                 // bPreCullCheck
		);
	}

	// Play sound
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			ExplosionSound,
			GetActorLocation(),
			1.0f,   // VolumeMultiplier
			1.0f,   // PitchMultiplier
			0.0f,   // StartTime
			nullptr // Attenuation
		);
	}

	// Hide mesh after explosion
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
	}

	// Disable collision
	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Stop movement
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
	}
}

void AGrenadeProjectile::StartDestroyTimer()
{
	// SERVER ONLY
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		DestroyTimerHandle,
		this,
		&AGrenadeProjectile::DestroyGrenade,
		DestroyDelay,
		false // No loop
	);

	UE_LOG(LogTemp, Log, TEXT("AGrenadeProjectile::StartDestroyTimer - Will destroy in %.1fs"), DestroyDelay);
}

void AGrenadeProjectile::DestroyGrenade()
{
	// SERVER ONLY - Destroy() replicates cleanup to clients
	if (HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("AGrenadeProjectile::DestroyGrenade - Destroying actor"));
		Destroy();
	}
}

float AGrenadeProjectile::GetFuseTimeRemaining() const
{
	if (bHasExploded)
	{
		return 0.0f;
	}

	if (!GetWorldTimerManager().IsTimerActive(FuseTimerHandle))
	{
		return 0.0f;
	}

	return GetWorldTimerManager().GetTimerRemaining(FuseTimerHandle);
}
