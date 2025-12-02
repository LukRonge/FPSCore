// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/GrenadeProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrenadeProjectile, Log, All);

AGrenadeProjectile::AGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replication setup - projectile must replicate to all clients
	bReplicates = true;
	bAlwaysRelevant = true;

	// Create collision component (root)
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(5.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	CollisionComponent->SetGenerateOverlapEvents(true);
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
	UE_LOG(LogGrenadeProjectile, Log, TEXT("InitializeProjectile - %s - Instigator=%s, Location=%s, Rotation=%s, HasAuthority=%d"),
		*GetName(),
		InInstigator ? *InInstigator->GetName() : TEXT("NULL"),
		*GetActorLocation().ToString(),
		*GetActorRotation().ToString(),
		HasAuthority());

	// Store instigator for damage attribution
	InstigatorPawn = InInstigator;

	// Log ProjectileMovement state
	if (ProjectileMovement)
	{
		UE_LOG(LogGrenadeProjectile, Log, TEXT("InitializeProjectile - ProjectileMovement: InitialSpeed=%.1f, MaxSpeed=%.1f, Velocity=%s, bShouldBounce=%d"),
			ProjectileMovement->InitialSpeed,
			ProjectileMovement->MaxSpeed,
			*ProjectileMovement->Velocity.ToString(),
			ProjectileMovement->bShouldBounce);
	}
	else
	{
		UE_LOG(LogGrenadeProjectile, Error, TEXT("InitializeProjectile - ProjectileMovement is NULL!"));
	}

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

		UE_LOG(LogGrenadeProjectile, Log, TEXT("InitializeProjectile - Fuse started, %.1fs until explosion"), FuseTime);
	}
}

void AGrenadeProjectile::OnFuseExpired()
{
	UE_LOG(LogGrenadeProjectile, Log, TEXT("OnFuseExpired - %s - Location=%s, HasAuthority=%d, bHasExploded=%d"),
		*GetName(), *GetActorLocation().ToString(), HasAuthority(), bHasExploded);

	// SERVER ONLY
	if (!HasAuthority())
	{
		UE_LOG(LogGrenadeProjectile, Warning, TEXT("OnFuseExpired - Not authority, skipping"));
		return;
	}

	// Prevent double explosion
	if (bHasExploded)
	{
		UE_LOG(LogGrenadeProjectile, Warning, TEXT("OnFuseExpired - Already exploded, skipping"));
		return;
	}

	UE_LOG(LogGrenadeProjectile, Log, TEXT("OnFuseExpired - Exploding at %s"), *GetActorLocation().ToString());

	// Apply damage (server authoritative)
	ApplyExplosionDamage();

	// Set replicated state - triggers OnRep on clients
	bHasExploded = true;

	// Play effects on all clients
	UE_LOG(LogGrenadeProjectile, Log, TEXT("OnFuseExpired - Calling Multicast_PlayExplosionEffects"));
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
		nullptr,                   // DamageTypeClass (uses default)
		TArray<AActor*>(),         // IgnoreActors (none)
		this,                      // DamageCauser
		InstigatorController,      // InstigatorController
		ECollisionChannel::ECC_Visibility  // DamagePreventionChannel
	);

	UE_LOG(LogTemp, Log, TEXT("AGrenadeProjectile::ApplyExplosionDamage - Damage: %.1f, Radius: %.1f"), ExplosionDamage, ExplosionRadius);
}

void AGrenadeProjectile::Multicast_PlayExplosionEffects_Implementation()
{
	UE_LOG(LogGrenadeProjectile, Log, TEXT("Multicast_PlayExplosionEffects - %s - Location=%s, ExplosionVFX=%s"),
		*GetName(), *GetActorLocation().ToString(),
		ExplosionVFX ? *ExplosionVFX->GetName() : TEXT("NULL"));

	// Play VFX
	if (ExplosionVFX)
	{
		UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ExplosionVFX,
			GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(1.0f),             // Scale
			true,                      // bAutoDestroy
			true,                      // bAutoActivate
			ENCPoolMethod::AutoRelease // Pooling
		);
		UE_LOG(LogGrenadeProjectile, Log, TEXT("Multicast_PlayExplosionEffects - Spawned VFX: %s"),
			NiagaraComp ? TEXT("SUCCESS") : TEXT("FAILED"));
	}
	else
	{
		UE_LOG(LogGrenadeProjectile, Warning, TEXT("Multicast_PlayExplosionEffects - No ExplosionVFX set!"));
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
