// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HoldableItemInterface.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "FPSPlayerController.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Kismet/KismetSystemLibrary.h"

AFPSCharacter::AFPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	// Pawn rotates with controller YAW only
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Configure Capsule Component
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);

	// Cache Character Movement Component reference
	CMC = GetCharacterMovement();

	// Note: Movement speed is initialized in BeginPlay() to use Blueprint-configured values

	// Configure main mesh (third person body)
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetOwnerNoSee(true);

	// Create skeleton hierarchy scene components
	Spine_03 = CreateDefaultSubobject<USceneComponent>(TEXT("spine_03"));
	Spine_03->SetupAttachment(GetMesh());

	Spine_04 = CreateDefaultSubobject<USceneComponent>(TEXT("spine_04"));
	Spine_04->SetupAttachment(Spine_03);

	Spine_05 = CreateDefaultSubobject<USceneComponent>(TEXT("spine_05"));
	Spine_05->SetupAttachment(Spine_04);

	Neck_01 = CreateDefaultSubobject<USceneComponent>(TEXT("neck_01"));
	Neck_01->SetupAttachment(Spine_05);

	// Create Camera component
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Neck_01);
	Camera->SetRelativeLocation(FVector(15.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;  // Camera follows spine hierarchy rotations

	// Create Arms mesh component (first person)
	Arms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arms"));
	Arms->SetupAttachment(Camera);
	Arms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Arms->SetOnlyOwnerSee(true);
	Arms->CastShadow = true;
	Arms->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create Legs mesh component (first person)
	Legs = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Legs"));
	Legs->SetupAttachment(GetMesh());
	Legs->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Legs->SetOnlyOwnerSee(true);
	Legs->CastShadow = true;
	Legs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

	// Find bone indices
	const int32 Spine03Index = RefSkeleton.FindBoneIndex(FName(TEXT("spine_03")));
	const int32 Spine04Index = RefSkeleton.FindBoneIndex(FName(TEXT("spine_04")));
	const int32 Spine05Index = RefSkeleton.FindBoneIndex(FName(TEXT("spine_05")));
	const int32 Neck01Index = RefSkeleton.FindBoneIndex(FName(TEXT("neck_01")));

	if (Spine03Index == INDEX_NONE || Spine04Index == INDEX_NONE ||
		Spine05Index == INDEX_NONE || Neck01Index == INDEX_NONE)
	{
		return;
	}

	// Build component space transforms cache - O(n) instead of O(n²)
	TArray<FTransform> ComponentSpaceTransforms;
	ComponentSpaceTransforms.SetNum(RefBonePose.Num());
	ComponentSpaceTransforms[0] = RefBonePose[0]; // Root bone

	for (int32 i = 1; i < RefBonePose.Num(); i++)
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(i);
		ComponentSpaceTransforms[i] = RefBonePose[i] * ComponentSpaceTransforms[ParentIndex];
	}

	// Get component space transforms for our bones
	const FTransform& Spine03Transform = ComponentSpaceTransforms[Spine03Index];
	const FTransform& Spine04Transform = ComponentSpaceTransforms[Spine04Index];
	const FTransform& Spine05Transform = ComponentSpaceTransforms[Spine05Index];
	const FTransform& Neck01Transform = ComponentSpaceTransforms[Neck01Index];

	// Mesh is Y forward, scene components are X forward - rotate 90° Yaw
	const FQuat InverseRotation = FRotator(0.0f, -90.0f, 0.0f).Quaternion();

	// Set Spine_03
	Spine_03->SetRelativeLocation(Spine03Transform.GetLocation());
	Spine_03->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	// Helper to set child component relative to parent bone
	auto SetChildRelativeTransform = [&](USceneComponent* Child, const FTransform& ChildBone, const FTransform& ParentBone)
	{
		FVector Delta = ChildBone.GetLocation() - ParentBone.GetLocation();
		Child->SetRelativeLocation(InverseRotation.RotateVector(Delta));
		Child->SetRelativeRotation(FRotator::ZeroRotator);
	};

	// Set child components
	SetChildRelativeTransform(Spine_04, Spine04Transform, Spine03Transform);
	SetChildRelativeTransform(Spine_05, Spine05Transform, Spine04Transform);
	SetChildRelativeTransform(Neck_01, Neck01Transform, Spine05Transform);

	// Arms at Mesh position - properly accumulate through hierarchy with rotations
	// Build transform chain from Mesh to Camera
	FTransform MeshToCamera = FTransform::Identity;
	MeshToCamera = FTransform(Spine_03->GetRelativeRotation(), Spine_03->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Spine_04->GetRelativeRotation(), Spine_04->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Spine_05->GetRelativeRotation(), Spine_05->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Neck_01->GetRelativeRotation(), Neck_01->GetRelativeLocation()) * MeshToCamera;
	MeshToCamera = FTransform(Camera->GetRelativeRotation(), Camera->GetRelativeLocation()) * MeshToCamera;

	// Arms needs to be offset by inverse of accumulated transform in Camera local space
	FVector CameraOffsetInMeshSpace = MeshToCamera.GetLocation();
	FVector ArmsLocation = MeshToCamera.GetRotation().Inverse().RotateVector(-CameraOffsetInMeshSpace);

	Arms->SetRelativeLocation(ArmsLocation);
	Arms->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// Store the calculated location as default hands offset
	DefaultHandsOffset = ArmsLocation;
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Initialize movement speed with current mode (uses Blueprint-configured speeds)
	UpdateMovementSpeed(CurrentMovementMode);

	// Setup first-person hands (only for locally controlled player)
	// This handles initial weapon positioning in first-person view
	if (IsLocallyControlled())
	{
		SetupHandsLocation();
	}
}

void AFPSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AFPSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate Pitch to all clients (owner calculates locally, simulated proxies use replicated value)
	DOREPLIFETIME_CONDITION(AFPSCharacter, Pitch, COND_SkipOwner);

	// Replicate ActiveItem to all clients
	DOREPLIFETIME(AFPSCharacter, ActiveItem);

	// Replicate CurrentMovementMode to all clients
	DOREPLIFETIME(AFPSCharacter, CurrentMovementMode);

	// Replicate gameplay state
	DOREPLIFETIME(AFPSCharacter, bIsDeath);
	DOREPLIFETIME(AFPSCharacter, bIsAiming);

	// NOTE: HandsOffset is NOT replicated - it's local to owning client for first-person arms only
}

void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DeltaSeconds = DeltaTime;

	// ActualSpeed is now calculated on-demand via GetActualSpeed()
}

void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind Enhanced Input actions
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Clear any existing bindings for this object to prevent duplicates
		EnhancedInputComponent->ClearBindingsForObject(this);

		// Bind separate Yaw and Pitch actions
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
	}
}

void AFPSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Camera is now set up in FPSPlayerController::OnPossess()
	// No need for Client RPC
}

void AFPSCharacter::UnPossessed()
{
	Super::UnPossessed();
}

void AFPSCharacter::UpdatePitch(float Y)
{
	// Calculate new pitch: (Y * -1) + current Pitch
	float NewPitch = (Y * -1.0f) + Pitch;

	// Clamp the angle between -45 and 45 degrees
	NewPitch = FMath::ClampAngle(NewPitch, -45.0f, 45.0f);

	// Only update on locally controlled characters
	if (IsLocallyControlled())
	{
		Pitch = NewPitch;

		// Update spine component rotations (visual feedback)
		UpdateSpineRotations();

		// Send to server if we're a client
		if (!HasAuthority())
		{
			Server_UpdatePitch(Pitch);
		}
	}
}

void AFPSCharacter::Server_UpdatePitch_Implementation(float NewPitch)
{
	// Server receives pitch from client and validates
	Pitch = FMath::ClampAngle(NewPitch, -45.0f, 45.0f);

	// Update spine components on server
	// Note: Pitch property replicates to other clients → triggers OnRep_Pitch
	UpdateSpineRotations();
}

void AFPSCharacter::UpdateSpineRotations()
{
	// Helper function to update spine component rotations based on current Pitch
	// Called from: UpdatePitch, Server_UpdatePitch, OnRep_Pitch
	//
	// Pitch is distributed across spine bones for natural upper body rotation:
	// - Spine_03: 40% Pitch influence (base rotation 90° Yaw is constant)
	// - Spine_04: 50% Pitch
	// - Spine_05: 60% Pitch
	// - Neck_01:  20% Pitch
	//
	// FRotator parameter order: (Pitch, Yaw, Roll)
	// Spine hierarchy simulates spine curve so player looks ahead of feet when looking down

	Spine_03->SetRelativeRotation(FRotator(Pitch * 0.4f, 90.0f, 0.0f));
	Spine_04->SetRelativeRotation(FRotator(Pitch * 0.5f, 0.0f, 0.0f));
	Spine_05->SetRelativeRotation(FRotator(Pitch * 0.6f, 0.0f, 0.0f));
	Neck_01->SetRelativeRotation(FRotator(Pitch * 0.2f, 0.0f, 0.0f));
}

void AFPSCharacter::SetupHandsLocation()
{
	// This function should ONLY run on locally controlled client
	// Arms mesh is OnlyOwnerSee - other players don't need this
	if (!IsLocallyControlled())
	{
		return;
	}

	// Calculate hands offset based on active item
	if (ActiveItem && ActiveItem->Implements<UHoldableItemInterface>())
	{
		HandsOffset = IHoldableItemInterface::Execute_GetHandsOffset(ActiveItem);
	}
	else
	{
		HandsOffset = DefaultHandsOffset;
	}

	// Apply to first-person arms mesh
	if (Arms)
	{
		Arms->SetRelativeLocation(HandsOffset);
	}
}

void AFPSCharacter::LookYaw(const FInputActionValue& Value)
{
	// Check if character is dead
	if (bIsDeath)
	{
		return;
	}

	// Get yaw input (float value from Mouse X axis)
	float YawValue = Value.Get<float>();

	// Enhanced Input sensitivity scaling
	float Sensitivity = 0.5f;
	if (bIsAiming)
	{
		Sensitivity *= 0.5f; // Half sensitivity when aiming
	}

	// Update yaw (horizontal camera rotation) - LEFT/RIGHT mouse movement
	AddControllerYawInput(YawValue * Sensitivity);
}

void AFPSCharacter::LookPitch(const FInputActionValue& Value)
{
	// Check if character is dead
	if (bIsDeath)
	{
		return;
	}

	// Get pitch input (float value from Mouse Y axis)
	float PitchValue = Value.Get<float>();

	// Enhanced Input sensitivity scaling
	float Sensitivity = 0.5f;
	if (bIsAiming)
	{
		Sensitivity *= 0.5f; // Half sensitivity when aiming
	}

	// Update pitch (vertical camera rotation + upper body aim) - UP/DOWN mouse movement
	UpdatePitch(PitchValue * Sensitivity);
}

void AFPSCharacter::Move(const FInputActionValue& Value)
{
	// Check if character is dead
	if (bIsDeath)
	{
		return;
	}

	// Get raw input from Enhanced Input (range -1 to 1, NOT unit length)
	FVector2D RawInput = Value.Get<FVector2D>();

	// Normalize to unit length for desired direction
	float InputMagnitude = RawInput.Size();

	if (InputMagnitude > KINDA_SMALL_NUMBER)
	{
		// Store normalized input
		CurrentMovementVector = RawInput / InputMagnitude;
	}
	else
	{
		// No input
		CurrentMovementVector = FVector2D::ZeroVector;
	}

	// Apply sprint direction clamping if in sprint mode
	if (CurrentMovementMode == EFPSMovementMode::Sprint && CMC)
	{
		float SprintMultiplier = GetSprintDirectionMultiplier(CurrentMovementVector);
		float ClampedSpeed = FMath::Lerp(JogSpeed, SprintSpeed, SprintMultiplier);
		CMC->MaxWalkSpeed = ClampedSpeed;
	}

	// Get actor forward and right vectors (synced with controller rotation)
	const FVector ForwardDirection = GetActorForwardVector();
	const FVector RightDirection = GetActorRightVector();

	// Build 3D world-space movement direction
	const FVector MovementDirection = (ForwardDirection * CurrentMovementVector.Y) + (RightDirection * CurrentMovementVector.X);

	// Add movement input - CMC handles acceleration, speed limiting, and network replication
	AddMovementInput(MovementDirection, 1.0f);
}

void AFPSCharacter::MoveCanceled(const FInputActionValue& Value)
{
	// Called when all movement keys are released
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
	UpdateMovementSpeed(EFPSMovementMode::Sprint);
	Server_SetMovementMode(EFPSMovementMode::Sprint);
}

void AFPSCharacter::SprintReleased()
{
	UpdateMovementSpeed(EFPSMovementMode::Jog);
	Server_SetMovementMode(EFPSMovementMode::Jog);
}

void AFPSCharacter::CrouchPressed()
{
	UpdateMovementSpeed(EFPSMovementMode::Crouch);
	Server_SetMovementMode(EFPSMovementMode::Crouch);
}

void AFPSCharacter::CrouchReleased()
{
	UpdateMovementSpeed(EFPSMovementMode::Jog);
	Server_SetMovementMode(EFPSMovementMode::Jog);
}

void AFPSCharacter::Server_SetMovementMode_Implementation(EFPSMovementMode NewMode)
{
	UpdateMovementSpeed(NewMode);
}

void AFPSCharacter::UpdateMovementSpeed(EFPSMovementMode NewMode)
{
	if (!CMC) return;

	// Validate transitions
	if (bIsDeath) return;

	// Set new movement mode
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
	// Called on clients when Pitch replicates from server
	// Update spine component rotations based on replicated Pitch value
	// Skip for locally controlled (already updated in UpdatePitch)
	if (!IsLocallyControlled())
	{
		UpdateSpineRotations();
	}
}

void AFPSCharacter::OnRep_ActiveItem()
{
	// Called on clients when ActiveItem changes
	// Update first-person hands position (only for locally controlled player)
	if (!HasAuthority())
	{
		SetupHandsLocation();  // Reuse shared logic, handles IsLocallyControlled check
	}
}

void AFPSCharacter::OnRep_IsDeath()
{
	// Called on clients when bIsDeath changes
	// Handle death state changes (animations, ragdoll, etc.)
	if (bIsDeath)
	{
		// Death logic - can be extended in Blueprint
	}
}

void AFPSCharacter::OnRep_CurrentMovementMode()
{
	// Called on clients when CurrentMovementMode replicates from server
	// Update MaxWalkSpeed based on replicated movement mode
	UpdateMovementSpeed(CurrentMovementMode);
}

float AFPSCharacter::GetSprintDirectionMultiplier(const FVector2D& MovementInput) const
{
	// No input = no multiplier
	if (MovementInput.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// Y component represents forward/backward movement
	// Y > 0.3: Forward or Forward-diagonal = full sprint (1.0)
	// Y <= 0.3: Side or Back movement = jog speed (0.0)
	const float ForwardThreshold = 0.3f;

	if (MovementInput.Y > ForwardThreshold)
	{
		// Forward movement (F, FL, FR) - full sprint speed
		return 1.0f;
	}
	else
	{
		// Side/Back movement (L, R, B, BL, BR) - jog speed
		return 0.0f;
	}
}

FVector2D AFPSCharacter::GetAnimMovementVector() const
{
	if (!CMC)
	{
		return FVector2D::ZeroVector;
	}

	// NOTE: This function uses DIFFERENT coordinate convention than input!
	// - INPUT convention (SmoothedMovementVector): X=right, Y=forward
	// - ANIM BLENDSPACE convention (return value): X=forward, Y=right
	//
	// This is intentional because Unreal BlendSpace standard is:
	//   Horizontal Axis (X-param) = Forward/Backward movement
	//   Vertical Axis (Y-param) = Right/Left movement

	// Get world velocity from Character Movement Component
	FVector WorldVelocity = GetVelocity();

	// Convert to local space (relative to actor rotation)
	// Unreal coordinate system: X=forward, Y=right, Z=up
	FVector LocalVelocity = GetActorRotation().UnrotateVector(WorldVelocity);

	// Extract 2D movement for AnimBP BlendSpace
	// AnimBP BlendSpace convention: X=forward, Y=right (matches Unreal actor space)
	FVector2D MovementVector(LocalVelocity.X, LocalVelocity.Y);

	// Normalize direction
	float Speed = MovementVector.Size();
	if (Speed < KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}

	FVector2D NormalizedDirection = MovementVector / Speed;

	// Determine multiplier based on movement mode
	float Multiplier = 1.0f;

	switch (CurrentMovementMode)
	{
		case EFPSMovementMode::Walk:
			Multiplier = 1.0f;
			break;

		case EFPSMovementMode::Jog:
			Multiplier = 2.0f;
			break;

		case EFPSMovementMode::Sprint:
			// Check if moving forward (X > 0.3 in AnimBP convention)
			if (NormalizedDirection.X > 0.3f)
			{
				// Full sprint for forward movement
				Multiplier = 3.0f;
			}
			else
			{
				// Clamp to jog for side/back movement
				Multiplier = 2.0f;
			}
			break;

		case EFPSMovementMode::Crouch:
			Multiplier = 1.0f;
			break;
	}

	// Return normalized direction scaled by multiplier
	return NormalizedDirection * Multiplier;
}
