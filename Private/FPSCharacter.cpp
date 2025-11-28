// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Interfaces/HoldableInterface.h"
#include "Interfaces/InteractableInterface.h"
#include "Interfaces/PickupableInterface.h"
#include "Interfaces/ItemCollectorInterface.h"
#include "Interfaces/UsableInterface.h"
#include "Interfaces/SightInterface.h"
#include "Interfaces/PlayerHUDInterface.h"
#include "Interfaces/ReloadableInterface.h"
#include "BaseWeapon.h"
#include "Core/FPSGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "FPSPlayerController.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/HUD.h"
#include "DrawDebugHelpers.h"
#include "Components/InventoryComponent.h"
#include "Components/HealthComponent.h"
#include "Components/RecoilComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Engine/DamageEvents.h"

AFPSCharacter::AFPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	// Pawn rotates with controller YAW only
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
	// Capsule should IGNORE projectiles - only skeletal mesh should detect bullet hits for bone damage
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

	CMC = GetCharacterMovement();
	CMC->MaxWalkSpeedCrouched = 150.0f;  // Same as WalkSpeed

	// Third person body - visible to others only
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetComponentTickEnabled(true);

	Spine_03 = CreateDefaultSubobject<USceneComponent>(TEXT("spine_03"));
	Spine_03->SetupAttachment(GetMesh());
	Spine_04 = CreateDefaultSubobject<USceneComponent>(TEXT("spine_04"));
	Spine_04->SetupAttachment(Spine_03);
	Spine_05 = CreateDefaultSubobject<USceneComponent>(TEXT("spine_05"));
	Spine_05->SetupAttachment(Spine_04);
	Neck_01 = CreateDefaultSubobject<USceneComponent>(TEXT("neck_01"));
	Neck_01->SetupAttachment(Spine_05);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Neck_01);
	Camera->SetRelativeLocation(FVector(15.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;

	// First person arms - owner only
	Arms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arms"));
	Arms->SetupAttachment(Camera);
	Arms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Arms->SetOnlyOwnerSee(true);
	Arms->CastShadow = true;
	Arms->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// First person legs - owner only
	Legs = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Legs"));
	Legs->SetupAttachment(GetMesh());
	Legs->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Legs->SetOnlyOwnerSee(true);
	Legs->CastShadow = true;
	Legs->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	RecoilComp = CreateDefaultSubobject<URecoilComponent>(TEXT("RecoilComponent"));

	// ============================================
	// BONE DAMAGE MULTIPLIERS MOVED TO UHealthComponent
	// ============================================
	// BoneDamageMultipliers initialization moved to UHealthComponent constructor
	// See: Components/HealthComponent.cpp
}

void AFPSCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	InitializeSpineComponents();
}

void AFPSCharacter::InitializeSpineComponents()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = MeshComp->GetSkeletalMeshAsset()->GetRefSkeleton();
	const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();

	const int32 Spine03Index = RefSkeleton.FindBoneIndex(FName(TEXT("spine_03")));
	const int32 Spine04Index = RefSkeleton.FindBoneIndex(FName(TEXT("spine_04")));
	const int32 Spine05Index = RefSkeleton.FindBoneIndex(FName(TEXT("spine_05")));
	const int32 Neck01Index = RefSkeleton.FindBoneIndex(FName(TEXT("neck_01")));

	if (Spine03Index == INDEX_NONE || Spine04Index == INDEX_NONE ||
		Spine05Index == INDEX_NONE || Neck01Index == INDEX_NONE)
	{
		return;
	}

	TArray<FTransform> ComponentSpaceTransforms;
	ComponentSpaceTransforms.SetNum(RefBonePose.Num());
	ComponentSpaceTransforms[0] = RefBonePose[0];

	for (int32 i = 1; i < RefBonePose.Num(); i++)
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(i);
		ComponentSpaceTransforms[i] = RefBonePose[i] * ComponentSpaceTransforms[ParentIndex];
	}

	const FTransform& Spine03Transform = ComponentSpaceTransforms[Spine03Index];
	const FTransform& Spine04Transform = ComponentSpaceTransforms[Spine04Index];
	const FTransform& Spine05Transform = ComponentSpaceTransforms[Spine05Index];
	const FTransform& Neck01Transform = ComponentSpaceTransforms[Neck01Index];

	const FQuat InverseRotation = FRotator(0.0f, -90.0f, 0.0f).Quaternion();

	Spine_03->SetRelativeLocation(Spine03Transform.GetLocation());
	Spine_03->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	// Store original Spine_03 location for crouch offset
	OriginalSpineLocation = Spine_03->GetRelativeLocation();

	auto SetChildRelativeTransform = [&](USceneComponent* Child, const FTransform& ChildBone, const FTransform& ParentBone)
	{
		FVector Delta = ChildBone.GetLocation() - ParentBone.GetLocation();
		Child->SetRelativeLocation(InverseRotation.RotateVector(Delta));
		Child->SetRelativeRotation(FRotator::ZeroRotator);
	};

	SetChildRelativeTransform(Spine_04, Spine04Transform, Spine03Transform);
	SetChildRelativeTransform(Spine_05, Spine05Transform, Spine04Transform);
	SetChildRelativeTransform(Neck_01, Neck01Transform, Spine05Transform);

	FTransform MeshToCamera = FTransform::Identity;
	MeshToCamera = FTransform(Spine_03->GetRelativeRotation(), Spine_03->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Spine_04->GetRelativeRotation(), Spine_04->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Spine_05->GetRelativeRotation(), Spine_05->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Neck_01->GetRelativeRotation(), Neck_01->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Camera->GetRelativeRotation(), Camera->GetRelativeLocation()) * MeshToCamera;

	FVector CameraOffsetInMeshSpace = MeshToCamera.GetLocation();
	FVector ArmsLocation = MeshToCamera.GetRotation().Inverse().RotateVector(-CameraOffsetInMeshSpace);

	Arms->SetRelativeLocation(ArmsLocation);
	Arms->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	DefaultArmsOffset = ArmsLocation;
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateMovementSpeed(CurrentMovementMode);

	// Link default animation layer (runs on ALL machines - each machine links its own AnimInstance)
	UpdateItemAnimLayer(nullptr);

	// Initialize Arms position (LOCAL ONLY)
	if (IsLocallyControlled())
	{
		ArmsOffset = DefaultArmsOffset;
		// Note: Arms position is now set in Tick() via interpolation

		Camera->SetFieldOfView(DefaultFOV);

		// Initialize default crosshair if no active item
		if (!ActiveItem && Controller && Controller->Implements<UPlayerHUDInterface>())
		{
			IPlayerHUDInterface::Execute_SetCrossHair(Controller, DefaultCrossHair, nullptr);
		}
	}

	// ============================================
	// INVENTORY DELEGATES (SERVER ONLY)
	// ============================================
	// Inventory events only fire on server (inventory changes are server-authoritative)
	// Clients receive updates via Items array replication
	if (HasAuthority() && InventoryComp)
	{
		InventoryComp->OnItemAdded.AddDynamic(this, &AFPSCharacter::OnInventoryItemAdded);
		InventoryComp->OnItemRemoved.AddDynamic(this, &AFPSCharacter::OnInventoryItemRemoved);
		InventoryComp->OnInventoryCleared.AddDynamic(this, &AFPSCharacter::OnInventoryCleared);
	}

	// ============================================
	// HEALTH DELEGATES (ALL MACHINES)
	// ============================================
	// Health delegates broadcast on BOTH server and clients:
	// - SERVER: ApplyDamage() → immediate delegate broadcast
	// - CLIENTS: OnRep_Health/OnRep_IsDeath → delegate broadcast when replicated
	// Therefore, ALL machines must bind to receive these events
	if (HealthComp)
	{
		// OnHealthChanged: Fires on SERVER (ApplyDamage) + CLIENTS (OnRep_Health)
		// Used for UI updates on owning client
		HealthComp->OnHealthChanged.AddDynamic(this, &AFPSCharacter::OnHealthComponentHealthChanged);

		// OnDeath: Fires on SERVER (ApplyDamage) + CLIENTS (OnRep_IsDeath)
		// Used for death camera/UI on owning client + ragdoll on all clients
		HealthComp->OnDeath.AddDynamic(this, &AFPSCharacter::OnHealthComponentDeath);

		// OnDamaged: Fires ONLY on SERVER (ApplyDamage)
		// Triggers Multicast_HitReaction RPC, so only server needs binding
		if (HasAuthority())
		{
			HealthComp->OnDamaged.AddDynamic(this, &AFPSCharacter::OnHealthComponentDamaged);
			HealthComp->ResetHealthState();
		}

		// ============================================
		// INITIALIZE UI WITH CURRENT HEALTH (ALL LOCALLY CONTROLLED)
		// ============================================
		// After binding delegates, manually initialize UI for locally controlled characters
		// This ensures health bar shows initial value (100/100) on spawn
		// - SERVER: ResetHealthState() already broadcasts, but this ensures UI sync
		// - CLIENTS: No ResetHealthState() call, so this is critical for initial UI
		if (IsLocallyControlled())
		{
			OnHealthComponentHealthChanged(HealthComp->Health);
		}
	}
}

void AFPSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AFPSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AFPSCharacter, Pitch, COND_SkipOwner);
	DOREPLIFETIME(AFPSCharacter, ActiveItem);
	DOREPLIFETIME(AFPSCharacter, CurrentMovementMode);
	DOREPLIFETIME(AFPSCharacter, bIsAiming);
	// Note: Health and bIsDeath moved to HealthComp
	// Note: InventoryComp has its own replication (Items array)
}

void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DeltaSeconds = DeltaTime;

	// ============================================
	// SPRINT INTENT ACTIVATION (LOCAL ONLY)
	// ============================================
	// Automatically activate/deactivate Sprint mode based on sprint intent + movement direction
	if (IsLocallyControlled())
	{
		if (bSprintIntentActive)
		{
			float SprintMultiplier = GetSprintDirectionMultiplier(CurrentMovementVector);

			if (SprintMultiplier > 0.0f && CurrentMovementMode != EFPSMovementMode::Sprint)
			{
				UpdateMovementSpeed(EFPSMovementMode::Sprint);
				Server_SetMovementMode(EFPSMovementMode::Sprint);
			}
			else if (SprintMultiplier <= 0.0f && CurrentMovementMode == EFPSMovementMode::Sprint)
			{
				UpdateMovementSpeed(EFPSMovementMode::Jog);
				Server_SetMovementMode(EFPSMovementMode::Jog);
			}
		}
	}

	// Check for interactable objects (only for locally controlled player)
	if (IsLocallyControlled())
	{
		CheckInteractionTrace();

		// Update aiming state (responds to item state changes like reload ending)
		UpdateAimingState();

		InterpolatedArmsOffset = CalculateInterpolatedArmsOffset(DeltaTime);
		LeanVector = CalculateLeanVector(DeltaTime);
		FVector BreathingVector = CalculateBreathing(DeltaTime);

		// Combine aiming offset + weapon lean/sway + breathing
		// LeanVector.X = Forward/back offset (cm)
		// LeanVector.Y = Lateral left/right offset (cm)
		// LeanVector.Z = Vertical up/down offset (cm)
		// BreathingVector = Idle breathing offset (XYZ)
		FVector FinalArmsOffset = InterpolatedArmsOffset;
		FinalArmsOffset.X += LeanVector.X + BreathingVector.X;
		FinalArmsOffset.Y += LeanVector.Y + BreathingVector.Y;
		FinalArmsOffset.Z += LeanVector.Z + BreathingVector.Z;

		Arms->SetRelativeLocation(FinalArmsOffset);

		// ============================================
		// BREATHING & LEANING CAMERA OFFSET (ADS ONLY)
		// ============================================
		// Apply breathing rotation + leaning position to camera when aiming
		// This creates realistic aim wobble and weapon sway during ADS
		if (bIsAiming && Camera)
		{
			BreathingRotation = CalculateBreathingRotation(BreathingVector);
			Camera->SetRelativeRotation(BreathingRotation);
			Camera->SetRelativeLocation(BaseCameraLocation + LeanVector);
		}
		else
		{
			if (Camera && (!BreathingRotation.IsZero() || Camera->GetRelativeLocation() != BaseCameraLocation))
			{
				BreathingRotation = FRotator::ZeroRotator;
				Camera->SetRelativeRotation(BreathingRotation);
				Camera->SetRelativeLocation(BaseCameraLocation);
			}
		}

		// ============================================
		// UPDATE LEANING VISUAL FEEDBACK
		// ============================================
		UpdateLeaningVisualFeedback(BreathingVector);
	}
}

void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->ClearBindingsForObject(this);

		if (IA_Look_Yaw)
		{
			EnhancedInputComponent->BindAction(IA_Look_Yaw, ETriggerEvent::Triggered, this, &AFPSCharacter::LookYaw);
		}

		if (IA_Look_Pitch)
		{
			EnhancedInputComponent->BindAction(IA_Look_Pitch, ETriggerEvent::Triggered, this, &AFPSCharacter::LookPitch);
		}

		if (IA_Move)
		{
			EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AFPSCharacter::Move);
			EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Canceled, this, &AFPSCharacter::MoveCanceled);
		}

		if (IA_Walk)
		{
			EnhancedInputComponent->BindAction(IA_Walk, ETriggerEvent::Started, this, &AFPSCharacter::WalkPressed);
			EnhancedInputComponent->BindAction(IA_Walk, ETriggerEvent::Completed, this, &AFPSCharacter::WalkReleased);
		}

		if (IA_Sprint)
		{
			EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AFPSCharacter::SprintPressed);
			EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AFPSCharacter::SprintReleased);
		}

		if (IA_Crouch)
		{
			EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AFPSCharacter::CrouchPressed);
			EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &AFPSCharacter::CrouchReleased);
		}

		if (IA_Jump)
		{
			EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AFPSCharacter::JumpPressed);
			EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AFPSCharacter::JumpReleased);
		}

		if (IA_Interact)
		{
			EnhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Started, this, &AFPSCharacter::InteractPressed);
		}

		if (IA_Drop)
		{
			EnhancedInputComponent->BindAction(IA_Drop, ETriggerEvent::Started, this, &AFPSCharacter::DropPressed);
		}

		if (IA_Shoot)
		{
			EnhancedInputComponent->BindAction(IA_Shoot, ETriggerEvent::Started, this, &AFPSCharacter::UseStarted);
			EnhancedInputComponent->BindAction(IA_Shoot, ETriggerEvent::Completed, this, &AFPSCharacter::UseStopped);
			EnhancedInputComponent->BindAction(IA_Shoot, ETriggerEvent::Canceled, this, &AFPSCharacter::UseStopped);
		}

		if (IA_Aim)
		{
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Started, this, &AFPSCharacter::AimingPressed);
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Completed, this, &AFPSCharacter::AimingReleased);
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Canceled, this, &AFPSCharacter::AimingReleased);
		}

		if (IA_Reload)
		{
			EnhancedInputComponent->BindAction(IA_Reload, ETriggerEvent::Triggered, this, &AFPSCharacter::ReloadPressed);
		}
	}
}

void AFPSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	Client_OnPossessed();
}

void AFPSCharacter::UnPossessed()
{
	Super::UnPossessed();
}

void AFPSCharacter::Client_OnPossessed_Implementation()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Setup hands location (LOCAL operation for locally controlled player)
	// Arms mesh is OnlyOwnerSee, so this only affects local player
	SetupArmsLocation(nullptr);

	// NOTE: UpdateItemAnimLayer(nullptr) is already called in BeginPlay() on ALL machines
	// No need to call it again here (would be redundant)

	// Setup input mapping and view target on owning client
	// Cast to AFPSPlayerController is acceptable here - this is framework-level code
	// where FPSCharacter is specifically designed to work with FPSPlayerController
	// SetupInputMapping() is controller-specific initialization that cannot be abstracted
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->SetViewTarget(this);

		// Setup input mapping (FPSPlayerController specific)
		if (AFPSPlayerController* FPSPC = Cast<AFPSPlayerController>(PC))
		{
			FPSPC->SetupInputMapping();
		}
	}
}

void AFPSCharacter::UpdatePitch(float Y)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Update local pitch accumulator for spine rotations (visual only)
	float NewLocalPitch = (Y * -1.0f) + LocalPitchAccumulator;
	NewLocalPitch = FMath::ClampAngle(NewLocalPitch, -45.0f, 45.0f);
	LocalPitchAccumulator = NewLocalPitch;

	UpdateSpineRotations();

	// Calculate network pitch from actual camera direction (inverse calculation)
	// This ensures ViewPointProvider returns the same direction as local camera
	float CorrectedPitch = CalculateNetworkPitchFromCamera();

	Pitch = CorrectedPitch;
	if (!HasAuthority())
	{
		Server_UpdatePitch(Pitch);
	}
}

void AFPSCharacter::Server_UpdatePitch_Implementation(float NewPitch)
{
	// ✅ FIX: Store ACTUAL camera pitch from client (no artificial clamp to input range)
	// Client sends calculated pitch from CalculateNetworkPitchFromCamera()
	// which includes spine chain amplification (can exceed ±45° input range)
	//
	// CRITICAL: Server needs ACTUAL camera pitch for accurate weapon ballistics
	// GetShootingViewPoint() uses this value for FireComponent->Shoot() direction
	//
	// Anti-cheat: Clamp to realistic camera pitch range (±90°), not input range (±45°)
	Pitch = FMath::Clamp(NewPitch, -90.0f, 90.0f);

	// ✅ FIX: Apply inverse compensation for server's spine rotations
	// Same logic as OnRep_Pitch() for remote clients
	//
	// Background:
	// - LocalPitchAccumulator is input-driven pitch (±45°) for spine animations
	// - Pitch is ACTUAL camera pitch (post-amplification, can be >45°) for shooting
	// - Spine chain amplifies: spine_03(40%) + spine_04(50%) + spine_05(60%) + neck_01(20%) ≈ 170%
	// - Inverse: camera_pitch / 1.7 ≈ LocalPitchAccumulator
	const float ProxyAimOffsetCompensation = 0.588f; // 1 / 1.7 ≈ 0.588
	LocalPitchAccumulator = Pitch * ProxyAimOffsetCompensation;
	LocalPitchAccumulator = FMath::Clamp(LocalPitchAccumulator, -45.0f, 45.0f);

	UpdateSpineRotations();
}

void AFPSCharacter::UpdateSpineRotations()
{
	Spine_03->SetRelativeRotation(FRotator(LocalPitchAccumulator * 0.4f, 90.0f, 0.0f));
	Spine_04->SetRelativeRotation(FRotator(LocalPitchAccumulator * 0.5f, 0.0f, 0.0f));
	Spine_05->SetRelativeRotation(FRotator(LocalPitchAccumulator * 0.6f, 0.0f, 0.0f));
	Neck_01->SetRelativeRotation(FRotator(LocalPitchAccumulator * 0.2f, 0.0f, 0.0f));
}

float AFPSCharacter::CalculateNetworkPitchFromCamera() const
{
	if (!Camera)
	{
		return 0.0f;
	}

	const FVector CameraForward = Camera->GetForwardVector();
	const FQuat ActorRotationQuat = GetActorQuat();
	const FVector LocalForward = ActorRotationQuat.Inverse().RotateVector(CameraForward);

	// Extract pitch from local forward vector
	// Pitch = atan2(Z, sqrt(X^2 + Y^2))
	const float HorizontalLength = FMath::Sqrt(LocalForward.X * LocalForward.X + LocalForward.Y * LocalForward.Y);
	const float PitchRadians = FMath::Atan2(LocalForward.Z, HorizontalLength);
	const float PitchDegrees = FMath::RadiansToDegrees(PitchRadians);

	// CRITICAL FIX: Do NOT clamp - spine chain amplifies rotation beyond ±45°
	// Server needs ACTUAL camera pitch for accurate weapon ballistics line traces
	// LocalPitchAccumulator is clamped to ±45° (input), but spine rotations amplify this
	// Example: LocalPitchAccumulator = 45° → spine rotations (40%+50%+60%+20%) → camera pitch ≈ 50°
	// Previous clamp to 45° caused: FPS view at 50° but server line trace only at 45° → mismatch
	return PitchDegrees;
}

FVector AFPSCharacter::CalculateInterpolatedArmsOffset(float DeltaTime)
{
	// ============================================
	// AIMING INTERPOLATION (LOCAL ONLY)
	// ============================================
	// This function is only called for locally controlled players
	// Handles smooth transition between hip fire and ADS

	float TargetAlpha = bIsAiming ? 1.0f : 0.0f;
	AimingAlpha = FMath::FInterpTo(AimingAlpha, TargetAlpha, DeltaTime, AimingInterpSpeed);

	FVector Result = FMath::Lerp(ArmsOffset, AimArmsOffset, AimingAlpha);

	// ============================================
	// CONTINUOUS INTERPOLATION (leaning and breathing scales)
	// ============================================
	// Smoothly interpolate leaning and breathing scales based on AimingAlpha
	CurrentLeaningScale = FMath::Lerp(HipLeaningScale, AimLeaningScale, AimingAlpha);
	CurrentBreathingScale = FMath::Lerp(HipBreathingScale, AimBreathingScale, AimingAlpha);

	// ============================================
	// AIMING THRESHOLD (AimingAlpha > 0.8)
	// ============================================
	// When interpolation reaches 80%, apply aiming state (FOV, look speed, hide FPS mesh)
	// This happens before full interpolation to provide snappy feel
	// Note: Crosshair is updated continuously in Tick() with LeanAlpha
	if (AimingAlpha > 0.8f && bIsAiming && !bAimingCrosshairSet && ActiveItem && ActiveItem->Implements<USightInterface>())
	{
		if (Camera)
		{
			float AimingFOV = ISightInterface::Execute_GetAimingFOV(ActiveItem);
			Camera->SetFieldOfView(AimingFOV);
		}

		CurrentLookSpeed = ISightInterface::Execute_GetAimLookSpeed(ActiveItem);

		if (ISightInterface::Execute_ShouldHideFPSMeshWhenAiming(ActiveItem) && Arms->IsVisible())
		{
			Arms->SetVisibility(false, true);

			// Hide weapon FPS mesh on locally controlled client
			if (ActiveItem->Implements<UHoldableInterface>())
			{
				IHoldableInterface::Execute_SetFPSMeshVisibility(ActiveItem, false);
			}
		}

		bAimingCrosshairSet = true;
	}

	return Result;
}

void AFPSCharacter::SetupArmsLocation(AActor* Item)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (Item && Item->Implements<UHoldableInterface>())
	{
		ArmsOffset = IHoldableInterface::Execute_GetArmsOffset(Item);
	}
	else
	{
		ArmsOffset = DefaultArmsOffset;
	}

	// Note: Arms position is now set in Tick() via interpolation
	// This function only updates ArmsOffset (base hip fire position)
}

void AFPSCharacter::UpdateItemAnimLayer(AActor* Item)
{
	// ============================================
	// ANIMATION LAYER SWITCHING (LOCAL operation)
	// ============================================
	// This function runs on ALL machines locally
	// Each machine manages its own AnimInstance layers
	// No replication needed - BeginPlay() or OnRep triggers this

	if (Item && Item->Implements<UHoldableInterface>())
	{
		// Item is valid and holdable - switch to item anim layer
		TSubclassOf<UAnimInstance> ItemAnimLayer = IHoldableInterface::Execute_GetAnimLayer(Item);

		if (ItemAnimLayer)
		{
			// Unlink default layer first (inline)
			if (DefaultAnimLayer)
			{
				GetMesh()->GetAnimInstance()->UnlinkAnimClassLayers(DefaultAnimLayer);
				Legs->GetAnimInstance()->UnlinkAnimClassLayers(DefaultAnimLayer);
				Arms->GetAnimInstance()->UnlinkAnimClassLayers(DefaultAnimLayer);
			}

			GetMesh()->GetAnimInstance()->LinkAnimClassLayers(ItemAnimLayer);
			Legs->GetAnimInstance()->LinkAnimClassLayers(ItemAnimLayer);
			Arms->GetAnimInstance()->LinkAnimClassLayers(ItemAnimLayer);
		}
		else
		{
			if (DefaultAnimLayer)
			{
				GetMesh()->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
				Legs->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
				Arms->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
			}
		}
	}
	else
	{
		if (DefaultAnimLayer)
		{
			GetMesh()->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
			Legs->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
			Arms->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
		}
	}
}

void AFPSCharacter::LookYaw(const FInputActionValue& Value)
{
	if (HealthComp && HealthComp->bIsDeath)
	{
		return;
	}

	float YawValue = Value.Get<float>();
	float BaseSensitivity = 0.5f;
	float Sensitivity = BaseSensitivity * (LookSpeed / 100.0f) * CurrentLookSpeed;

	AddControllerYawInput(YawValue * Sensitivity);
}

void AFPSCharacter::LookPitch(const FInputActionValue& Value)
{
	if (HealthComp && HealthComp->bIsDeath)
	{
		return;
	}

	float PitchValue = Value.Get<float>();
	float BaseSensitivity = 0.5f;
	float Sensitivity = BaseSensitivity * (LookSpeed / 100.0f) * CurrentLookSpeed;

	UpdatePitch(PitchValue * Sensitivity);
}

void AFPSCharacter::Move(const FInputActionValue& Value)
{
	if (HealthComp && HealthComp->bIsDeath)
	{
		return;
	}

	FVector2D RawInput = Value.Get<FVector2D>();
	float InputMagnitude = RawInput.Size();

	if (InputMagnitude > KINDA_SMALL_NUMBER)
	{
		CurrentMovementVector = RawInput / InputMagnitude;
	}
	else
	{
		CurrentMovementVector = FVector2D::ZeroVector;
	}

	// Note: Sprint direction clamping removed - handled automatically in Tick()
	// Sprint mode is now activated/deactivated based on bSprintIntentActive + movement direction

	const FVector ForwardDirection = GetActorForwardVector();
	const FVector RightDirection = GetActorRightVector();
	const FVector MovementDirection = (ForwardDirection * CurrentMovementVector.Y) + (RightDirection * CurrentMovementVector.X);

	AddMovementInput(MovementDirection, 1.0f);
}

void AFPSCharacter::MoveCanceled(const FInputActionValue& Value)
{
	CurrentMovementVector = FVector2D::ZeroVector;
}

void AFPSCharacter::WalkPressed()
{
	UpdateMovementSpeed(EFPSMovementMode::Walk);
	Server_SetMovementMode(EFPSMovementMode::Walk);
}

void AFPSCharacter::WalkReleased()
{
	UpdateMovementSpeed(EFPSMovementMode::Jog);
	Server_SetMovementMode(EFPSMovementMode::Jog);
}

void AFPSCharacter::SprintPressed()
{
	// Set sprint intent (actual Sprint mode activated in Tick if moving forward)
	bSprintIntentActive = true;
}

void AFPSCharacter::SprintReleased()
{
	bSprintIntentActive = false;

	if (CurrentMovementMode == EFPSMovementMode::Sprint)
	{
		UpdateMovementSpeed(EFPSMovementMode::Jog);
		Server_SetMovementMode(EFPSMovementMode::Jog);
	}
}

void AFPSCharacter::CrouchPressed()
{
	// Block crouch if actively sprinting (moving forward)
	if (CurrentMovementMode == EFPSMovementMode::Sprint && IsActivelyMoving())
	{
		return;
	}

	UpdateMovementSpeed(EFPSMovementMode::Crouch);
	Server_SetMovementMode(EFPSMovementMode::Crouch);
	Crouch();

	// Apply crouch camera offset (LOCAL ONLY - owning client)
	if (IsLocallyControlled())
	{
		Spine_03->SetRelativeLocation(OriginalSpineLocation + FVector(0.0f, 10.0f, -50.0f));
	}
}

void AFPSCharacter::CrouchReleased()
{
	UpdateMovementSpeed(EFPSMovementMode::Jog);
	Server_SetMovementMode(EFPSMovementMode::Jog);
	UnCrouch();

	// Restore original spine location (LOCAL ONLY - owning client)
	if (IsLocallyControlled())
	{
		Spine_03->SetRelativeLocation(OriginalSpineLocation);
	}
}

void AFPSCharacter::JumpPressed()
{
	Jump();
}

void AFPSCharacter::JumpReleased()
{
	StopJumping();
}

void AFPSCharacter::InteractPressed()
{
	// Only locally controlled players can initiate interactions
	// LastInteractableActor is already validated by CheckInteractionTrace()
	if (!IsLocallyControlled() || !IsValid(LastInteractableActor))
	{
		return;
	}

	// LastInteractableActor is guaranteed to implement IInteractableInterface
	// (set by CheckInteractionTrace only after full validation)
	IInteractableInterface* Interactable = Cast<IInteractableInterface>(LastInteractableActor);

	FInteractionContext Ctx;
	Ctx.Controller = GetController();
	Ctx.Pawn = this;
	Ctx.Instigator = this;

	TArray<FGameplayTag> Verbs;
	Interactable->Execute_GetVerbs(LastInteractableActor, Verbs, Ctx);

	if (Verbs.Num() > 0)
	{
		const FGameplayTag& FirstVerb = Verbs[0];

		if (Interactable->Execute_CanInteract(LastInteractableActor, FirstVerb, Ctx))
		{
			// INTERFACE-DRIVEN INTERACTION
			// Delegate to item's Interact() - item determines how to be interacted with
			// Item will call back to Character via IItemCollectorInterface if needed
			// Clean separation: items are active, interfaces provide contracts
			Interactable->Execute_Interact(LastInteractableActor, FirstVerb, Ctx);
		}
	}
}

void AFPSCharacter::DropPressed()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!ActiveItem) return;

	// Check if item can be unequipped via interface (no direct cast)
	// Blocks drop during reload, montage playing, etc.
	if (ActiveItem->Implements<UHoldableInterface>())
	{
		if (!IHoldableInterface::Execute_CanBeUnequipped(ActiveItem))
		{
			return;
		}
	}

	Server_DropItem(ActiveItem);
}

void AFPSCharacter::UseStarted()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Block fire if actively sprinting (moving forward)
	if (CurrentMovementMode == EFPSMovementMode::Sprint && IsActivelyMoving())
	{
		return;
	}

	if (!ActiveItem || !ActiveItem->Implements<UUsableInterface>())
	{
		return;
	}

	FUseContext Ctx;
	IUsableInterface::Execute_UseStart(ActiveItem, Ctx);
}

void AFPSCharacter::UseStopped()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!ActiveItem || !ActiveItem->Implements<UUsableInterface>())
	{
		return;
	}

	FUseContext Ctx;
	IUsableInterface::Execute_UseStop(ActiveItem, Ctx);
}

void AFPSCharacter::AimingPressed()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Block aim if actively sprinting (moving forward)
	if (CurrentMovementMode == EFPSMovementMode::Sprint && IsActivelyMoving())
	{
		return;
	}

	// Set input state and let UpdateAimingState decide if we should actually aim
	bAimInputHeld = true;
	UpdateAimingState();
}

void AFPSCharacter::AimingReleased()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Clear input state and update aiming
	bAimInputHeld = false;
	UpdateAimingState();
}

bool AFPSCharacter::CanPerformAiming() const
{
	// Character-level blocking (for future: obstacle, ladder, swimming)
	if (AimBlockingFlags != 0)
	{
		return false;
	}

	// Must have active item
	if (!ActiveItem)
	{
		return false;
	}

	// Item must support aiming (have sight)
	if (!ActiveItem->Implements<USightInterface>())
	{
		return false;
	}

	// Item-level blocking via interface (reload, overheat, etc.)
	if (ActiveItem->Implements<UHoldableInterface>())
	{
		if (!IHoldableInterface::Execute_CanAim(ActiveItem))
		{
			return false;
		}
	}

	return true;
}

void AFPSCharacter::UpdateAimingState()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	bool bShouldAim = bAimInputHeld && CanPerformAiming();

	// No change needed
	if (bShouldAim == bIsAiming)
	{
		return;
	}

	if (bShouldAim)
	{
		// ============================================
		// START AIMING
		// ============================================

		// Get sight actor for aiming point calculation
		AActor* SightActor = ISightInterface::Execute_GetSightActor(ActiveItem);
		if (!SightActor)
		{
			return;
		}

		// Calculate arms offset for aiming
		// Goal: Align sight's AimingPoint with camera center (0,0,0) in camera space
		FVector AimingPointLocal = ISightInterface::Execute_GetAimingPoint(ActiveItem);
		FVector CurrentArmsOffset = Arms->GetRelativeLocation();

		// Transform AimingPoint: SightActor local space -> World space -> Camera local space
		FVector AimingPointWorld = SightActor->GetActorTransform().TransformPosition(AimingPointLocal);
		FVector AimingPointInCameraSpace = Camera->GetComponentTransform().InverseTransformPosition(AimingPointWorld);

		// Calculate new Arms offset: Move Arms to align AimingPoint with camera center
		AimArmsOffset = CurrentArmsOffset - AimingPointInCameraSpace;

		// Get aiming parameters from sight
		AimLeaningScale = ISightInterface::Execute_GetAimLeaningScale(ActiveItem);
		AimBreathingScale = ISightInterface::Execute_GetAimBreathingScale(ActiveItem);

		// Notify item about aiming state
		if (ActiveItem->Implements<UHoldableInterface>())
		{
			IHoldableInterface::Execute_SetAiming(ActiveItem, true);
		}

		// Set local state immediately for responsive feel
		bIsAiming = true;

		// Notify server to replicate to other clients
		Server_SetAiming(true);
	}
	else
	{
		// ============================================
		// STOP AIMING
		// ============================================

		Arms->SetVisibility(true, true);
		Camera->SetFieldOfView(DefaultFOV);
		CurrentLookSpeed = 1.0f;

		// Restore weapon FPS mesh visibility on locally controlled client
		if (ActiveItem && ActiveItem->Implements<UHoldableInterface>())
		{
			IHoldableInterface::Execute_SetFPSMeshVisibility(ActiveItem, true);
		}

		// Notify item about aiming state
		if (ActiveItem && ActiveItem->Implements<UHoldableInterface>())
		{
			IHoldableInterface::Execute_SetAiming(ActiveItem, false);
		}

		// Set local state immediately for responsive feel
		bIsAiming = false;
		bAimingCrosshairSet = false;

		// Notify server to replicate to other clients
		Server_SetAiming(false);
	}
}

void AFPSCharacter::ReloadPressed()
{
	if (!IsLocallyControlled()) return;
	if (!ActiveItem) return;

	AActor* Item = ActiveItem;

	if (!Item->Implements<UReloadableInterface>()) return;
	if (!IReloadableInterface::Execute_CanReload(Item)) return;

	FUseContext Ctx;
	Ctx.Controller = GetController();
	Ctx.Pawn = this;

	IReloadableInterface::Execute_Reload(Item, Ctx);
}

bool AFPSCharacter::CanReload() const
{
	// Get Arms AnimInstance
	if (!Arms)
	{
		return false;
	}

	UAnimInstance* AnimInst = Arms->GetAnimInstance();
	if (!AnimInst)
	{
		return false;
	}

	// Check montage slot (reload uses "UpperBody" slot)
	if (AnimInst->IsSlotActive("UpperBody"))
	{
		return false;
	}

	// Check death state via HealthComponent
	if (HealthComp && HealthComp->bIsDeath)
	{
		return false;
	}

	// Future features (not yet implemented):
	// TODO: Add ladder climbing check when ladder system is implemented
	// TODO: Add traversal action check (climbing over obstacles)

	return true;
}

bool AFPSCharacter::IsActivelyMoving() const
{
	if (!CMC)
	{
		return false;
	}

	// ShouldMove calculation (from AnimBP logic)
	float VelocitySize = GetVelocity().Size();
	float NormalizedVelocity = VelocitySize / 150.0f;
	bool bHasVelocity = NormalizedVelocity > 0.01f;

	FVector CurrentAcceleration = CMC->GetCurrentAcceleration();
	bool bHasAcceleration = !CurrentAcceleration.IsNearlyZero();

	return bHasVelocity && bHasAcceleration;
}

void AFPSCharacter::Server_SetMovementMode_Implementation(EFPSMovementMode NewMode)
{
	UpdateMovementSpeed(NewMode);
}

void AFPSCharacter::Server_SetAiming_Implementation(bool bNewAiming)
{
	// SERVER ONLY - set authoritative aiming state
	// This will replicate to all clients via bIsAiming REPLICATED property
	bIsAiming = bNewAiming;
}

void AFPSCharacter::UpdateMovementSpeed(EFPSMovementMode NewMode)
{
	if (!CMC) return;

	if (HealthComp && HealthComp->bIsDeath) return;

	CurrentMovementMode = NewMode;

	switch (CurrentMovementMode)
	{
		case EFPSMovementMode::Walk:
			CMC->MaxWalkSpeed = WalkSpeed;
			break;
		case EFPSMovementMode::Jog:
			CMC->MaxWalkSpeed = JogSpeed;
			break;
		case EFPSMovementMode::Sprint:
			CMC->MaxWalkSpeed = SprintSpeed;
			break;
		case EFPSMovementMode::Crouch:
			CMC->MaxWalkSpeed = CrouchSpeed;
			break;
	}
}

void AFPSCharacter::OnRep_Pitch()
{
	// Only update spine rotations for remote clients (not locally controlled)
	// Locally controlled client already updates spine via UpdatePitch()
	if (!IsLocallyControlled())
	{
		// PROXY AIMOFFSET COMPENSATION:
		// Replicated Pitch is now ACTUAL camera pitch (after spine chain amplification)
		// But AimOffset in AnimBP needs the original input-driven pitch range (±45°)
		// Apply inverse compensation to approximate LocalPitchAccumulator from camera pitch
		//
		// Spine chain calculation: spine_03(40%) + spine_04(50%) + spine_05(60%) + neck_01(20%)
		// These are RELATIVE rotations, so they accumulate: ~170% amplification (empirical)
		// Inverse: camera_pitch / 1.7 ≈ LocalPitchAccumulator
		//
		// NOTE: Tune this multiplier empirically by comparing local vs proxy AimOffset max range
		// Goal: Proxy reaches max AimOffset (6) at same visual angle as local client
		const float ProxyAimOffsetCompensation = 0.588f; // 1 / 1.7 ≈ 0.588

		LocalPitchAccumulator = Pitch * ProxyAimOffsetCompensation;
		LocalPitchAccumulator = FMath::Clamp(LocalPitchAccumulator, -45.0f, 45.0f);

		UpdateSpineRotations();
	}
}

void AFPSCharacter::OnRep_ActiveItem(AActor* OldActiveItem)
{
	// OnRep runs on CLIENTS when ActiveItem replicates from server
	// Follows OnRep pattern: Clients execute SAME local logic as server

	// 1. Unequip old item (if exists)
	if (OldActiveItem)
	{
		UnEquipItem(OldActiveItem);
	}

	// 2. Equip new item (if exists)
	if (ActiveItem)
	{
		EquipItem(ActiveItem);
	}
}



// OnRep_IsDeath moved to UHealthComponent

void AFPSCharacter::OnRep_CurrentMovementMode()
{
	UpdateMovementSpeed(CurrentMovementMode);
}

float AFPSCharacter::GetSprintDirectionMultiplier(const FVector2D& MovementInput) const
{
	if (MovementInput.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float ForwardThreshold = 0.3f;

	// Sprint only when moving forward (F, FL, FR)
	return MovementInput.Y > ForwardThreshold ? 1.0f : 0.0f;
}

// ============================================
// INTERACTION SYSTEM
// ============================================

void AFPSCharacter::CheckInteractionTrace()
{
	if (!Camera)
	{
		return;
	}

	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector CameraDirection = Camera->GetForwardVector();

	// Start trace 50cm forward to avoid hitting own character
	const FVector TraceStart = CameraLocation + CameraDirection * 50.0f;
	const FVector TraceEnd = CameraLocation + CameraDirection * InteractionDistance;

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (bHit && Hit.GetActor() && Hit.GetActor()->Implements<UInteractableInterface>())
	{
		AActor* HitActor = Hit.GetActor();
		IInteractableInterface* Interactable = Cast<IInteractableInterface>(HitActor);

		FInteractionContext Ctx;
		Ctx.Controller = GetController();
		Ctx.Pawn = this;
		Ctx.Instigator = this;
		Ctx.Hit = Hit;

		TArray<FGameplayTag> Verbs;
		Interactable->Execute_GetVerbs(HitActor, Verbs, Ctx);

		if (Verbs.Num() > 0)
		{
			const FGameplayTag& FirstVerb = Verbs[0];
			bool bCanInteract = Interactable->Execute_CanInteract(HitActor, FirstVerb, Ctx);

			if (bCanInteract)
			{
				FText InteractionText = Interactable->Execute_GetInteractionText(HitActor, FirstVerb, Ctx);

				if (Controller && Controller->Implements<UPlayerHUDInterface>())
				{
					IPlayerHUDInterface::Execute_UpdateItemInfo(Controller, InteractionText.ToString());
				}

				LastInteractableActor = HitActor;
				return;
			}
		}
	}

	if (LastInteractableActor != nullptr)
	{
		if (Controller && Controller->Implements<UPlayerHUDInterface>())
		{
			IPlayerHUDInterface::Execute_UpdateItemInfo(Controller, FString());
		}

		LastInteractableActor = nullptr;
	}
}

// ============================================
// INVENTORY SYSTEM IMPLEMENTATION
// ============================================

void AFPSCharacter::Server_PickupItem_Implementation(AActor* Item)
{
	// SERVER VALIDATION (anti-cheat, race conditions)
	// Client already validated before sending RPC, but server MUST re-validate
	// to prevent cheating and handle race conditions

	if (!Item || !HasAuthority())
	{
		return;
	}

	if (InventoryComp->AddItem(Item))
	{

	}
}

void AFPSCharacter::Server_DropItem_Implementation(AActor* Item)
{
	// SERVER VALIDATION (anti-cheat)
	// Client already has local checks, but server MUST re-validate

	if (!Item || !HasAuthority())
	{
		return;
	}

	if (!InventoryComp->ContainsItem(Item)) return;

	// Server re-validates busy state (anti-cheat)
	// Blocks drop during reload, montage playing, etc.
	if (Item->Implements<UHoldableInterface>())
	{
		if (!IHoldableInterface::Execute_CanBeUnequipped(Item))
		{
			return;
		}
	}

	InventoryComp->RemoveItem(Item);
}



// ============================================
// IItemCollectorInterface Implementation
// ============================================

void AFPSCharacter::Pickup_Implementation(AActor* Item)
{
	if (!Item) return;
	if (!Item->Implements<UPickupableInterface>()) return;

	FInteractionContext Ctx;
	Ctx.Controller = GetController();
	Ctx.Pawn = this;
	Ctx.Instigator = this;

	if (!IPickupableInterface::Execute_CanBePicked(Item, Ctx)) return;

	Server_PickupItem(Item);
}

void AFPSCharacter::Drop_Implementation(AActor* Item)
{
	// TODO: Implement drop logic
}

AActor* AFPSCharacter::GetActiveItem_Implementation() const
{
	return ActiveItem;
}

void AFPSCharacter::UnEquipItem(AActor* Item)
{
	if (!IsValid(Item)) return;
	if (!Item->Implements<UHoldableInterface>()) return;

	// Reset animation layer (MUST run even if item detached - prevents leak)
	UpdateItemAnimLayer(nullptr);

	if (IsLocallyControlled())
	{
		SetupArmsLocation(nullptr);

		if (!ActiveItem && Controller && Controller->Implements<UPlayerHUDInterface>())
		{
			IPlayerHUDInterface::Execute_SetCrossHair(Controller, DefaultCrossHair, nullptr);
			IPlayerHUDInterface::Execute_UpdateActiveItem(Controller, nullptr);
		}

		// Reset all scales to defaults when no active item (defensive cleanup)
		if (!ActiveItem)
		{
			HipLeaningScale = 1.0f;
			HipBreathingScale = 1.0f;
			// Current* scales will interpolate to Hip* scales in CalculateInterpolatedArmsOffset()
		}
	}

	IHoldableInterface::Execute_OnUnequipped(Item);
}

void AFPSCharacter::EquipItem(AActor* Item)
{
	if (!IsValid(Item)) return;
	if (!Item->Implements<UHoldableInterface>()) return;

	if (UPrimitiveComponent* TPSMesh = IHoldableInterface::Execute_GetTPSMeshComponent(Item))
	{
		bool bItemInWorldState = TPSMesh->IsSimulatingPhysics() || !Item->IsHidden();
		if (bItemInWorldState)
		{
			PerformPickup(Item);
		}
	}

	// Mesh attachment (intentionally redundant - maintains dual FPS/TPS hierarchy)
	FName AttachSocket = IHoldableInterface::Execute_GetAttachSocket(Item);

	Item->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
	Item->SetActorRelativeTransform(FTransform::Identity);

	if (UPrimitiveComponent* TPSMesh = IHoldableInterface::Execute_GetTPSMeshComponent(Item))
	{
		TPSMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
		TPSMesh->SetRelativeTransform(FTransform::Identity);
	}

	if (UPrimitiveComponent* FPSMesh = IHoldableInterface::Execute_GetFPSMeshComponent(Item))
	{
		FPSMesh->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
		FPSMesh->SetRelativeTransform(FTransform::Identity);
	}

	Item->SetActorHiddenInGame(false);

	// LOCAL operations (each machine runs independently)
	UpdateItemAnimLayer(Item);

	if (IsLocallyControlled())
	{
		SetupArmsLocation(Item);
		HipLeaningScale = IHoldableInterface::Execute_GetLeaningScale(Item);
		HipBreathingScale = IHoldableInterface::Execute_GetBreathingScale(Item);
	}

	// HUD update (owning client only)
	if (IsLocallyControlled() && Controller && Controller->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateActiveItem(Controller, ActiveItem);

		if (Item && Item->Implements<USightInterface>())
		{
			TSubclassOf<UUserWidget> HipCrosshair = ISightInterface::Execute_GetCrossHair(Item);
			TSubclassOf<UUserWidget> AimCrosshair = ISightInterface::Execute_GetAimingCrosshair(Item);
			IPlayerHUDInterface::Execute_SetCrossHair(Controller, HipCrosshair, AimCrosshair);
		}
		else if (!ActiveItem)
		{
			IPlayerHUDInterface::Execute_SetCrossHair(Controller, DefaultCrossHair, nullptr);
		}
	}

	if (Item->Implements<UHoldableInterface>())
	{
		IHoldableInterface::Execute_OnEquipped(Item, this);
	}
}


// ============================================
// INVENTORY EVENT CALLBACKS (SERVER ONLY)
// ============================================

void AFPSCharacter::OnInventoryItemAdded(AActor* Item)
{
	if (!HasAuthority())
	{
		return;
	}

	Item->SetOwner(this);
	Multicast_PickupItem(Item);

	// Auto-equip if first item
	if (InventoryComp->GetItemCount() == 1)
	{
		ActiveItem = Item;  // Replicated property change (triggers OnRep on clients)

		// Server executes equip immediately (OnRep pattern)
		if (HasAuthority() && ActiveItem)
		{
			EquipItem(ActiveItem);
		}
	}

	// Notify item via interface (SERVER ONLY)
	if (Item->Implements<UPickupableInterface>())
	{
		FInteractionContext Ctx;
		Ctx.Controller = GetController();
		Ctx.Pawn = this;
		Ctx.Instigator = this;

		IPickupableInterface::Execute_OnPicked(Item, this, Ctx);
	}
}

void AFPSCharacter::Multicast_PickupItem_Implementation(AActor* Item)
{
	PerformPickup(Item);
}

void AFPSCharacter::PerformPickup(AActor* Item)
{
	// Physical pickup: Disable physics, attach to body, hide
	// Runs on ALL machines via Multicast RPC
	// Logical operations (animation, hands) handled by EquipItem()

	if (!IsValid(Item)) return;

	// Disable physics BEFORE attaching (prevents warning)
	if (Item->Implements<UHoldableInterface>())
	{
		if (UPrimitiveComponent* TPSMesh = IHoldableInterface::Execute_GetTPSMeshComponent(Item))
		{
			TPSMesh->SetSimulatePhysics(false);
			TPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TPSMesh->SetRelativeTransform(FTransform::Identity);
		}

		if (UPrimitiveComponent* FPSMesh = IHoldableInterface::Execute_GetFPSMeshComponent(Item))
		{
			FPSMesh->SetRelativeTransform(FTransform::Identity);
		}
	}

	FAttachmentTransformRules AttachRules(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::KeepWorld,
		false
	);
	Item->AttachToComponent(GetMesh(), AttachRules);
	Item->SetActorHiddenInGame(true);
}

void AFPSCharacter::OnInventoryItemRemoved(AActor* Item)
{
	if (!HasAuthority())
	{
		return;
	}

	// Conditional unequip - only if dropping active item
	if (Item == ActiveItem)
	{
		AActor* OldActiveItem = ActiveItem;
		ActiveItem = nullptr;  // Replicated property change (triggers OnRep on clients)

		// Server executes unequip immediately (OnRep pattern)
		if (HasAuthority() && OldActiveItem)
		{
			UnEquipItem(OldActiveItem);
		}
	}

	// Clear owner (replicated) - BEFORE Multicast
	Item->SetOwner(nullptr);

	// PHYSICAL DROP (all clients via Multicast)
	Multicast_DropItem(Item);

	// Notify item via interface (SERVER ONLY)
	if (Item->Implements<UPickupableInterface>())
	{
		FInteractionContext Ctx;
		Ctx.Controller = GetController();
		Ctx.Pawn = this;
		Ctx.Instigator = this;

		IPickupableInterface::Execute_OnDropped(Item, Ctx);
	}
}

void AFPSCharacter::OnInventoryCleared()
{
}

void AFPSCharacter::Multicast_DropItem_Implementation(AActor* Item)
{
	PerformDrop(Item);
}

void AFPSCharacter::PerformDrop(AActor* Item)
{
	if (!IsValid(Item)) return;

	FTransform DropTransform;
	FVector DropImpulse;
	GetDropTransformAndImpulse(Item, DropTransform, DropImpulse);

	// Restore mesh hierarchy (reattach FPS/TPS to ItemRoot)
	if (Item->Implements<UHoldableInterface>())
	{
		USceneComponent* ItemRoot = Item->GetRootComponent();
		FAttachmentTransformRules ReAttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			false
		);

		if (UPrimitiveComponent* TPSMesh = IHoldableInterface::Execute_GetTPSMeshComponent(Item))
		{
			TPSMesh->SetSimulatePhysics(false);
			TPSMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TPSMesh->AttachToComponent(ItemRoot, ReAttachRules);
			TPSMesh->SetRelativeTransform(FTransform::Identity);
		}

		if (UPrimitiveComponent* FPSMesh = IHoldableInterface::Execute_GetFPSMeshComponent(Item))
		{
			FPSMesh->AttachToComponent(ItemRoot, ReAttachRules);
			FPSMesh->SetRelativeTransform(FTransform::Identity);
		}
	}

	// Detach from character
	FDetachmentTransformRules DetachRules(
		EDetachmentRule::KeepWorld,
		EDetachmentRule::KeepWorld,
		EDetachmentRule::KeepWorld,
		false
	);
	Item->GetRootComponent()->DetachFromComponent(DetachRules);

	// Place in world and enable physics
	Item->SetActorTransform(DropTransform);
	Item->SetActorHiddenInGame(false);

	if (Item->Implements<UHoldableInterface>())
	{
		if (UPrimitiveComponent* TPSMesh = IHoldableInterface::Execute_GetTPSMeshComponent(Item))
		{
			TPSMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			TPSMesh->SetSimulatePhysics(true);
			TPSMesh->AddImpulse(DropImpulse, NAME_None, true);
		}
	}
}

void AFPSCharacter::GetDropTransformAndImpulse_Implementation(AActor* Item, FTransform& OutTransform, FVector& OutImpulse)
{
	// ✅ Use existing network-safe viewpoint function (same as shooting!)
	// This works on SERVER because Pitch is replicated and GetActorEyesViewPoint works on server
	FVector ViewLocation;
	FRotator ViewRotation;
	GetShootingViewPoint_Implementation(ViewLocation, ViewRotation);

	// Get camera direction from viewpoint rotation
	FVector CameraDirection = ViewRotation.Vector();

	// Calculate drop location:
	// Start from camera viewpoint, then offset forward and down to hand level
	// DropForwardDistance: ~30cm forward from camera (outside capsule collision)
	// DropUpwardOffset: Negative value to offset below camera to hand level (e.g., -20cm)
	FVector DropLocation = ViewLocation;
	DropLocation += CameraDirection * DropForwardDistance; // Forward offset from camera
	DropLocation.Z += DropUpwardOffset; // Vertical offset (negative = below camera)

	// Rotate item 90 degrees left (counterclockwise in Yaw axis)
	FRotator DropRotation = GetActorRotation();
	DropRotation.Yaw -= 90.0f;
	OutTransform = FTransform(DropRotation, DropLocation);

	// ✅ Calculate throw impulse using CAMERA DIRECTION (not actor forward!)
	// This allows throwing in any direction based on where player is looking (up/down/left/right)
	FVector ThrowDirection = CameraDirection + FVector(0, 0, DropUpwardArc);
	ThrowDirection.Normalize();

	// Base throw impulse
	FVector ThrowImpulse = ThrowDirection * DefaultDropImpulseStrength;

	// Add character velocity to impulse (inheritance)
	// When character is running forward, item inherits velocity + throw impulse
	// This prevents character from outrunning the dropped item
	if (CMC)
	{
		FVector CharacterVelocity = CMC->Velocity * VelocityInheritanceMultiplier;
		ThrowImpulse += CharacterVelocity;
	}

	OutImpulse = ThrowImpulse;
}

// ============================================
// LEANING SYSTEM IMPLEMENTATION (Postprocess Offset)
// ============================================
//
// Self-contained function that calculates weapon sway based on movement
// All parameters are hardcoded for simplicity
// State is tracked via static variables (per-instance via this pointer map)
// Returns 3D offset: X = forward/back (cm), Y = lateral left/right (cm), Z = vertical up/down (cm)

FVector AFPSCharacter::CalculateLeanVector(float DeltaTime)
{
	// ============================================
	// HARDCODED PARAMETERS (REALISTIC VALUES)
	// ============================================
	// Based on research: Insurgency, Escape from Tarkov, Ready or Not, CS:GO
	// TUNED: Refined values for smoother forward/backward and lateral movement

	// Velocity-based lean (weapon follows movement direction)
	const float MaxLateralOffset = 3.8f;        // cm - lateral sway (tuned: was 4.5, original 1.2)
	const float MaxForwardOffset = 1.2f;        // cm - forward sway (tuned: was 2.5, original 0.6)

	// Walking bob (perpendicular oscillation)
	const float BobAmplitudeHorizontal = 2.4f;  // cm - horizontal bob (tuned: was 2.8, original 0.8)
	const float BobAmplitudeVertical = 3.5f;    // cm - vertical bob (not returned)
	const float BobFrequency = 8.0f;            // cycles per jog speed

	// Mouse input influence (weapon tilt/lag)
	const float MouseTiltStrength = 7.0f;       // cm - tilt effect (tuned: was 12.0, more controlled)
	const float MouseLagSpeed = 6.0f;           // spring damping (tuned: was 5.0, faster response)

	// Interpolation speed
	const float LeanInterpSpeed = 8.0f;         // velocity-based lean smoothing

	// ============================================
	// GET VELOCITY (local space)
	// ============================================
	FVector WorldVelocity = CMC->Velocity;
	WorldVelocity.Z = 0.0f; // Ignore vertical movement

	// Transform to local space (X=forward, Y=right)
	FQuat ActorRot = GetActorQuat();
	FVector LocalVelocity = ActorRot.Inverse().RotateVector(WorldVelocity);
	float VelocityMag = LocalVelocity.Size();

	// ============================================
	// UPDATE WALK CYCLE TIME (for bob phase)
	// ============================================
	if (VelocityMag > 10.0f)
	{
		float NormalizedSpeed = VelocityMag / 450.0f; // Normalize to jog speed
		LeanState_WalkCycleTime += DeltaTime * BobFrequency * NormalizedSpeed;
	}
	// Don't reset when stopping - preserves phase, prevents jarring snap

	// ============================================
	// CALCULATE VELOCITY-BASED LEAN (follow movement direction)
	// ============================================
	float VelLateral = FMath::Clamp(LocalVelocity.Y / 450.0f, -1.0f, 1.0f);  // Normalize
	float VelForward = FMath::Clamp(LocalVelocity.X / 450.0f, -1.0f, 1.0f);  // Normalize

	// Lean follows velocity (moving RIGHT → lean RIGHT, moving FORWARD → lean FORWARD)
	float VelocityLateralLean = VelLateral * MaxLateralOffset;
	float VelocityForwardLean = VelForward * MaxForwardOffset;

	// ============================================
	// CALCULATE PERPENDICULAR BOB (walking cycle)
	// ============================================
	// Bob perpendicular to movement direction (creates natural "swagger")
	float BobHorizontal = 0.0f;
	float BobForward = 0.0f;

	if (VelocityMag > 10.0f)
	{
		// Normalize velocity direction
		FVector2D VelDir;
		VelDir.X = LocalVelocity.X / VelocityMag;  // Forward component
		VelDir.Y = LocalVelocity.Y / VelocityMag;  // Right component

		// Forward movement (W) → horizontal bob (left-right swing)
		BobHorizontal = FMath::Sin(LeanState_WalkCycleTime) * BobAmplitudeHorizontal;
		BobHorizontal *= FMath::Abs(VelDir.X); // Scale by forward component

		// Strafe movement (A/D) → horizontal bob (perpendicular to strafe)
		// FIX: Both forward AND strafe create horizontal bob (not forward bob)
		float StrafeBobHorizontal = FMath::Sin(LeanState_WalkCycleTime) * BobAmplitudeHorizontal * 0.7f;
		StrafeBobHorizontal *= FMath::Abs(VelDir.Y); // Scale by strafe component
		BobHorizontal += StrafeBobHorizontal;

		// Vertical bob component (subtle forward/back during walk cycle)
		// TUNED: Reduced from 0.3 to 0.15 for gentler forward/back oscillation
		BobForward = FMath::Cos(LeanState_WalkCycleTime * 2.0f) * (BobAmplitudeHorizontal * 0.15f);
	}

	// ============================================
	// CALCULATE VERTICAL BOB (Z-axis)
	// ============================================
	// Primary component: Up/down bounce during walk cycle
	// Frequency: 1× horizontal bob (one full cycle per walk cycle)
	// TUNED: Reduced from 2.0× to 1.0× for slower, more natural rhythm
	float VerticalBob = 0.0f;

	if (VelocityMag > 10.0f)
	{
		// Sin wave at same frequency as horizontal bob (smooth, slow bounce)
		// Always positive (weapon goes DOWN, not up - realistic foot impact)
		float NormalizedSpeed = FMath::Clamp(VelocityMag / 450.0f, 0.0f, 1.0f);
		VerticalBob = FMath::Abs(FMath::Sin(LeanState_WalkCycleTime * 1.0f)) * BobAmplitudeVertical;
		VerticalBob *= NormalizedSpeed;  // Scale by movement speed

		// Invert to negative (weapon drops DOWN on step)
		VerticalBob = -VerticalBob;
	}

	// ============================================
	// CALCULATE MOUSE INPUT INFLUENCE (weapon tilt/lag)
	// ============================================
	FVector2D MouseTiltOffset = FVector2D::ZeroVector;

	// Get current control rotation
	if (AController* Ctrl = GetController())
	{
		FRotator CurrentRotation = Ctrl->GetControlRotation();

		// Calculate mouse delta (angular velocity)
		FRotator DeltaRotation = CurrentRotation - LeanState_PreviousControlRotation;
		DeltaRotation.Normalize(); // Clamp to ±180°

		// Convert angular velocity to tilt offset
		// Yaw (horizontal mouse) → lateral offset (weapon lags left/right)
		// Pitch (vertical mouse) → forward offset (weapon lags up/down)
		float MouseYawDelta = DeltaRotation.Yaw / DeltaTime;   // degrees per second
		float MousePitchDelta = DeltaRotation.Pitch / DeltaTime; // degrees per second

		// Store RAW mouse delta magnitude for crosshair alpha
		// Combine yaw and pitch for total angular velocity
		LeanState_RawMouseDelta = FMath::Sqrt(MouseYawDelta * MouseYawDelta + MousePitchDelta * MousePitchDelta);

		// Target tilt based on mouse speed
		FVector2D TargetMouseTilt;
		TargetMouseTilt.X = FMath::Clamp(MousePitchDelta * 0.002f, -1.0f, 1.0f) * MouseTiltStrength;
		TargetMouseTilt.Y = FMath::Clamp(-MouseYawDelta * 0.002f, -1.0f, 1.0f) * MouseTiltStrength;

		// Spring interpolation (weapon lags behind camera)
		LeanState_MouseLagOffset = FMath::Vector2DInterpTo(LeanState_MouseLagOffset, TargetMouseTilt, DeltaTime, MouseLagSpeed);
		MouseTiltOffset = LeanState_MouseLagOffset;

		// Store rotation for next frame
		LeanState_PreviousControlRotation = CurrentRotation;
	}

	// ============================================
	// COMBINE ALL OFFSETS INTO TARGET
	// ============================================
	// NOTE: Breathing sway is now calculated separately in CalculateBreathing()
	// This keeps leaning (movement-based) separate from breathing (idle-based)
	FVector TargetOffset;
	TargetOffset.X = VelocityForwardLean + BobForward + MouseTiltOffset.X;
	TargetOffset.Y = VelocityLateralLean + BobHorizontal + MouseTiltOffset.Y;
	TargetOffset.Z = VerticalBob;  // Z = vertical bob only

	// Apply aiming scale (reduces all sway during ADS)
	TargetOffset *= CurrentLeaningScale;

	// ============================================
	// SMOOTH INTERPOLATION (final spring damping)
	// ============================================
	LeanState_CurrentLean = FMath::VInterpTo(LeanState_CurrentLean, TargetOffset, DeltaTime, LeanInterpSpeed);

	return LeanState_CurrentLean;
}

// ============================================
// BREATHING SYSTEM IMPLEMENTATION
// ============================================
//
// Standalone breathing calculation function
// Uses CurrentBreathingScale from ActiveItem weapon (hip-fire idle breathing)
// Interpolates to AimBreathingScale from sight when aiming (reduced breathing during ADS)
//
FVector AFPSCharacter::CalculateBreathing(float DeltaTime)
{
	// ============================================
	// HARDCODED PARAMETERS
	// ============================================
	const float IdleSwayAmplitude = 0.4f;       // cm - breathing amplitude
	const float IdleSwayFrequencyX = 0.45f;     // Hz - breathing rate X (27 cycles/min, 1 cycle per 2.2 sec)
	const float IdleSwayFrequencyY = 0.55f;     // Hz - breathing rate Y (33 cycles/min, 1 cycle per 1.8 sec)
	const float IdleSwayFrequencyZ = 0.35f;     // Hz - breathing rate Z (21 cycles/min, 1 cycle per 2.9 sec)
	const float IdleBlendSpeed = 3.0f;          // idle sway activation blend
	const float LeanInterpSpeed = 8.0f;         // smooth interpolation

	// ============================================
	// GET VELOCITY (local space)
	// ============================================
	FVector WorldVelocity = CMC->Velocity;
	WorldVelocity.Z = 0.0f; // Ignore vertical movement
	float VelocityMag = WorldVelocity.Size();

	// ============================================
	// IDLE SWAY ACTIVATION (blend based on velocity)
	// ============================================
	// When stationary: IdleActivation = 1.0 (full idle sway)
	// When moving: IdleActivation = 0.0 (disable idle sway)
	float TargetIdleActivation = (VelocityMag < 50.0f) ? 1.0f : 0.0f;
	BreathingState_IdleActivation = FMath::FInterpTo(BreathingState_IdleActivation, TargetIdleActivation, DeltaTime, IdleBlendSpeed);

	// ============================================
	// UPDATE IDLE SWAY TIME (always running)
	// ============================================
	BreathingState_IdleSwayTime += DeltaTime;

	// ============================================
	// CALCULATE IDLE SWAY (breathing/tremor)
	// ============================================
	FVector TargetBreathing = FVector::ZeroVector;

	if (BreathingState_IdleActivation > 0.01f)
	{
		// Sine waves at different frequencies (creates figure-8 pattern in XY, subtle Z breathing)
		float IdleX = FMath::Sin(BreathingState_IdleSwayTime * IdleSwayFrequencyX * 2.0f * PI);
		float IdleY = FMath::Sin(BreathingState_IdleSwayTime * IdleSwayFrequencyY * 2.0f * PI);
		float IdleZ = FMath::Cos(BreathingState_IdleSwayTime * IdleSwayFrequencyZ * 2.0f * PI);  // Slower breathing rate

		TargetBreathing.X = IdleX * IdleSwayAmplitude * BreathingState_IdleActivation;
		TargetBreathing.Y = IdleY * IdleSwayAmplitude * BreathingState_IdleActivation;
		TargetBreathing.Z = IdleZ * (IdleSwayAmplitude * 0.75f) * BreathingState_IdleActivation;  // 75% amplitude for Z

		// Apply breathing scale (CurrentBreathingScale interpolates hip → ADS)
		TargetBreathing *= CurrentBreathingScale;
	}

	// ============================================
	// SMOOTH INTERPOLATION (final spring damping)
	// ============================================
	BreathingState_CurrentBreathing = FMath::VInterpTo(BreathingState_CurrentBreathing, TargetBreathing, DeltaTime, LeanInterpSpeed);

	return BreathingState_CurrentBreathing;
}

// ============================================
// BREATHING ROTATION CALCULATION (Camera sway)
// ============================================
//
// Converts breathing offset (cm) to camera rotation (degrees)
// Used for realistic aim wobble during ADS
//
FRotator AFPSCharacter::CalculateBreathingRotation(const FVector& BreathingVector) const
{
	// ============================================
	// HARDCODED PARAMETERS
	// ============================================
	const float OffsetToRotationScale = 0.8f;   // degrees per cm (tuned for realistic wobble)

	// ============================================
	// CONVERT OFFSET TO ROTATION
	// ============================================
	// BreathingVector.Y (lateral offset, cm) → Yaw (horizontal rotation, degrees)
	// BreathingVector.Z (vertical offset, cm) → Pitch (vertical rotation, degrees)
	// Invert Z for natural pitch direction (positive Z up → negative Pitch down)

	float BreathingYaw = BreathingVector.Y * OffsetToRotationScale;
	float BreathingPitch = -BreathingVector.Z * OffsetToRotationScale;

	return FRotator(BreathingPitch, BreathingYaw, 0.0f);
}

// ============================================
// UPDATE LEANING VISUAL FEEDBACK (Material Parameter Collection + Crosshair)
// ============================================
//
// Shared function that updates both MPC shader effects and crosshair expansion
// Combines leaning and breathing vectors for shader, calculates lean alpha for crosshair
// LOCAL ONLY - called from Tick() for locally controlled players
//
void AFPSCharacter::UpdateLeaningVisualFeedback(const FVector& BreathingVector)
{
	// ============================================
	// MATERIAL PARAMETER COLLECTION UPDATE
	// ============================================
	// Send normalized vector to MPC for shader effects (independent scaling)
	if (MPC_Aim)
	{
		// Combine leaning and breathing vectors
		FVector CombinedOffset = LeanVector + BreathingVector;

		// Swap axes for shader convention: X→Y, Y→X (lateral↔forward)
		FVector ShaderSpaceOffset;
		ShaderSpaceOffset.X = CombinedOffset.Y; // Lateral → X
		ShaderSpaceOffset.Y = CombinedOffset.X; // Forward → Y
		ShaderSpaceOffset.Z = CombinedOffset.Z; // Vertical → Z

		// MPC-specific scaling (independent from leaning/breathing calculations)
		// Higher MaxOffset = gentler response, lower IntensityScale = reduced dispersion
		const float MaxOffsetCm = 50.0f;   // Baseline for normalization (increased for gentler response)
		const float IntensityScale = 0.7f; // Extra damping multiplier (70% of normalized value)

		// Normalize each axis independently to -1..1 range, then apply intensity damping
		FVector NormalizedOffset;
		NormalizedOffset.X = FMath::Clamp((ShaderSpaceOffset.X / MaxOffsetCm) * IntensityScale, -1.0f, 1.0f);
		NormalizedOffset.Y = FMath::Clamp((ShaderSpaceOffset.Y / MaxOffsetCm) * IntensityScale, -1.0f, 1.0f);
		NormalizedOffset.Z = FMath::Clamp((ShaderSpaceOffset.Z / MaxOffsetCm) * IntensityScale, -1.0f, 1.0f);

		// Set to MPC (each axis always in -1..1 range)
		UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), MPC_Aim, FName("OffsetDirection"), FLinearColor(NormalizedOffset));
	}

	// ============================================
	// CROSSHAIR UPDATE (based on character velocity + look input + recoil)
	// ============================================
	// Update crosshair dynamically based on aiming state and movement/look intensity
	if (Controller && Controller->Implements<UPlayerHUDInterface>())
	{
		// 1. Movement contribution (from character velocity)
		// Normalize by max sprint speed (650 cm/s)
		// Result: 0.0 = standing, 0.23 = walk, 0.69 = jog, 1.0 = sprint
		float Velocity = GetVelocity().Size();
		const float MaxSprintSpeed = 650.0f;
		float MovementAlpha = FMath::Clamp(Velocity / MaxSprintSpeed, 0.0f, 1.0f);

		// 2. Look input contribution (from RAW mouse delta)
		// Typical mouse speed: slow ~20 deg/s, medium ~100 deg/s, fast ~300 deg/s
		// Scaled down to prevent dominating movement alpha
		const float MaxMouseSpeed = 400.0f;  // Higher threshold for less sensitivity
		const float LookInfluence = 0.3f;     // Look contributes up to 30% of total alpha
		float LookAlpha = FMath::Clamp(LeanState_RawMouseDelta / MaxMouseSpeed, 0.0f, 1.0f) * LookInfluence;

		// 3. Recoil contribution (from shot accumulation)
		// ShotCount: 0 = no recoil, MaxAccumulation = sustained fire (full recoil penalty)
		// Weight: 35% of total alpha (significant impact on sustained fire)
		float RecoilAlpha = 0.0f;
		if (RecoilComp)
		{
			float RecoilFactor = FMath::Min(static_cast<float>(RecoilComp->ShotCount) / RecoilComp->MaxAccumulation, 1.0f);
			RecoilAlpha = RecoilFactor * 0.35f; // 35% weight
		}

		// 4. Combined alpha: additive (movement + look + recoil, clamped to 1.0)
		// When idle, no shots: Velocity = 0, RawMouseDelta = 0, ShotCount = 0 → LeanAlpha = 0
		// When walking: Velocity = 150, RawMouseDelta = 0, ShotCount = 0 → LeanAlpha = 0.23
		// When sprinting: Velocity = 650, RawMouseDelta = 0, ShotCount = 0 → LeanAlpha = 1.0
		// When standing + fast look: Velocity = 0, RawMouseDelta = 400, ShotCount = 0 → LeanAlpha = 0.3
		// When standing + sustained fire: Velocity = 0, RawMouseDelta = 0, ShotCount = 8 → LeanAlpha = 0.35
		float LeanAlpha = FMath::Clamp(MovementAlpha + LookAlpha + RecoilAlpha, 0.0f, 1.0f);

		// Update crosshair with aiming state and lean alpha
		IPlayerHUDInterface::Execute_UpdateCrossHair(Controller, bIsAiming, LeanAlpha);
	}
}

// ============================================
// IViewPointProviderInterface Implementation
// ============================================

void AFPSCharacter::GetShootingViewPoint_Implementation(FVector& OutLocation, FRotator& OutRotation) const
{
	// Get base eyes view point (Yaw from controller, Location from capsule + BaseEyeHeight)
	GetActorEyesViewPoint(OutLocation, OutRotation);

	// ✅ Override Pitch with our custom replicated value
	// This is critical because bUseControllerRotationPitch = false
	// Server needs replicated Pitch for accurate weapon ballistics
	OutRotation.Pitch = Pitch;

	// Optional: Use Camera location if available (more accurate than capsule + BaseEyeHeight)
	if (Camera)
	{
		OutLocation = Camera->GetComponentLocation();
	}
}

float AFPSCharacter::GetViewPitch_Implementation() const
{
	return Pitch;
}

float AFPSCharacter::GetRecoilFactor_Implementation() const
{
	// Get recoil accumulation factor from RecoilComponent
	// Used by weapons to increase spread during sustained fire
	// RecoilComponent tracks ShotCount independently on server and client (LOCAL state)
	if (RecoilComp)
	{
		// Calculate normalized recoil factor (0.0 = no shots, 1.0 = sustained fire)
		return FMath::Min(static_cast<float>(RecoilComp->ShotCount) / RecoilComp->MaxAccumulation, 1.0f);
	}

	return 0.0f; // No recoil if RecoilComp is null
}

// ============================================
// DAMAGE SYSTEM IMPLEMENTATION
// ============================================

float AFPSCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Delegate directly to HealthComponent for damage processing
	// HealthComponent handles: bone multipliers, health state, delegate broadcasting
	// FPSCharacter reacts to delegates for visual effects (hit reactions, ragdoll, UI)
	if (HealthComp)
	{
		return HealthComp->ApplyDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	}

	// Fallback: No HealthComp (shouldn't happen in normal gameplay)
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

// ============================================
// HEALTH COMPONENT DELEGATE CALLBACKS
// ============================================

void AFPSCharacter::OnHealthComponentDamaged()
{
	// Called on SERVER when HealthComponent broadcasts OnDamaged delegate
	// Trigger multicast RPC for hit reaction on all clients

	if (HasAuthority())
	{
		Multicast_HitReaction();
	}
}

void AFPSCharacter::OnHealthComponentDeath()
{
	// Called on SERVER (ApplyDamage) and CLIENTS (OnRep_IsDeath)
	// Handle death visual effects

	if (HasAuthority())
	{
		// SERVER: Drop active item on death
		if (IsValid(ActiveItem) && InventoryComp)
		{
			InventoryComp->RemoveItem(ActiveItem);
		}

		Multicast_ProcessDeath();
		Client_ProcessDeath();
	}
}

void AFPSCharacter::OnHealthComponentHealthChanged(float NewHealth)
{
	if (IsLocallyControlled() && Controller && Controller->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_UpdateHealth(Controller, NewHealth);
	}
}

// ============================================
// HIT REACTION SYSTEM (Multicast RPC)
// ============================================

void AFPSCharacter::Multicast_HitReaction_Implementation()
{
	HitReaction();

	if (IsLocallyControlled() && Controller && Controller->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_AddDamageEffect(Controller);
	}
}

void AFPSCharacter::HitReaction()
{
	if (HitReactionMontages.Num() == 0)
	{
		return;
	}

	int32 RandomIndex = FMath::RandRange(0, HitReactionMontages.Num() - 1);
	UAnimMontage* SelectedMontage = HitReactionMontages[RandomIndex];

	if (!SelectedMontage)
	{
		return;
	}

	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(SelectedMontage);
	}

	if (Arms && Arms->GetAnimInstance())
	{
		Arms->GetAnimInstance()->Montage_Play(SelectedMontage);
	}
}

// ============================================
// DEATH SYSTEM (Client + Multicast RPCs)
// ============================================

void AFPSCharacter::Client_ProcessDeath_Implementation()
{
	if (Camera)
	{
		Camera->PostProcessSettings.bOverride_ColorSaturation = true;
		Camera->PostProcessSettings.ColorSaturation = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	if (Controller && Controller->Implements<UPlayerHUDInterface>())
	{
		IPlayerHUDInterface::Execute_SetHUDVisibility(Controller, false);
	}
}

void AFPSCharacter::Multicast_ProcessDeath_Implementation()
{
	EnableRagdoll();
}

// ============================================
// RECOIL SYSTEM
// ============================================

void AFPSCharacter::ApplyRecoilKick_Implementation(float RecoilScale)
{
	if (!HasAuthority())
	{
		return;
	}

	if (RecoilComp)
	{
		RecoilComp->AddRecoil(RecoilScale);
	}
	else
	{
		return;
	}

	Multicast_ApplyRecoil(RecoilScale);
}

void AFPSCharacter::Multicast_ApplyRecoil_Implementation(float RecoilScale)
{
	if (!RecoilComp)
	{
		return;
	}

	if (IsLocallyControlled())
	{
		RecoilComp->ApplyRecoilToCamera(RecoilScale);
	}
	else
	{
		RecoilComp->ApplyRecoilToWeapon(RecoilScale);
	}
}

void AFPSCharacter::ApplyCameraPitchKick_Implementation(float PitchDelta)
{
	// Apply pitch kick (negative = camera looks up)
	UpdatePitch(PitchDelta);
}

void AFPSCharacter::ApplyCameraYawKick_Implementation(float YawDelta)
{
	// Apply yaw kick via controller input
	AddControllerYawInput(YawDelta);
}

bool AFPSCharacter::IsAimingDownSights_Implementation() const
{
	return bIsAiming;
}

bool AFPSCharacter::IsLocalPlayer_Implementation() const
{
	return IsLocallyControlled();
}

// ============================================
// RAGDOLL SYSTEM
// ============================================

void AFPSCharacter::EnableRagdoll()
{
	if (CMC)
	{
		CMC->SetMovementMode(MOVE_None);
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (GetMesh())
	{
		GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetMesh()->SetAllBodiesBelowSimulatePhysics(FName("pelvis"), true, true);
	}
}

void AFPSCharacter::DisableRagdoll()
{
	if (CMC)
	{
		CMC->SetMovementMode(MOVE_Walking);
	}

	if (GetMesh())
	{
		GetMesh()->SetCollisionProfileName(FName("CharacterMesh"), true);
		GetMesh()->SetCollisionObjectType(ECC_Pawn);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetMesh()->SetAllBodiesSimulatePhysics(false);
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCapsuleComponent()->SetCollisionProfileName(FName("Pawn"), true);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	}

	if (DefaultAnimLayer)
	{
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
		Legs->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
		Arms->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
	}
}

// ============================================
// ICharacterMeshProviderInterface IMPLEMENTATION
// ============================================

USkeletalMeshComponent* AFPSCharacter::GetBodyMesh_Implementation() const
{
	// Return character's primary mesh (Body, visible to all players)
	// This is the ACharacter::GetMesh() default mesh component
	return GetMesh();
}

USkeletalMeshComponent* AFPSCharacter::GetArmsMesh_Implementation() const
{
	// Return first-person arms mesh (visible only to owner)
	return Arms;
}

USkeletalMeshComponent* AFPSCharacter::GetLegsMesh_Implementation() const
{
	// Return first-person legs mesh (visible only to owner)
	return Legs;
}

