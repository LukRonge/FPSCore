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
#include "Interfaces/PlayerHUDInterface.h"
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
#include "Components/PrimitiveComponent.h"

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

	CMC = GetCharacterMovement();

	// Third person body - visible to others only
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetComponentTickEnabled(true);

	// Skeleton hierarchy for upper body aim
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

	// Inventory component
	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
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

	// Build component space transforms cache
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

	DefaultHandsOffset = ArmsLocation;
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateMovementSpeed(CurrentMovementMode);
	LinkDefaultLayer();
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
	DOREPLIFETIME(AFPSCharacter, bIsDeath);
	DOREPLIFETIME(AFPSCharacter, bIsAiming);
	// Note: InventoryComp has its own replication (Items array)
}

void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DeltaSeconds = DeltaTime;

	// Check for interactable objects (only for locally controlled player)
	if (IsLocallyControlled())
	{
		CheckInteractionTrace();
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

		if (IA_Interact)
		{
			EnhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Started, this, &AFPSCharacter::InteractPressed);
		}

		if (IA_Drop)
		{
			EnhancedInputComponent->BindAction(IA_Drop, ETriggerEvent::Started, this, &AFPSCharacter::DropPressed);
		}
	}
}

void AFPSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Cache player controller reference
	CachedPlayerController = Cast<AFPSPlayerController>(NewController);

	if (CachedPlayerController)
	{
		Client_OnPossessed();
	}
}

void AFPSCharacter::UnPossessed()
{
	Super::UnPossessed();

	// Clear cached player controller
	CachedPlayerController = nullptr;
}

void AFPSCharacter::Client_OnPossessed_Implementation()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	// Cache player controller on client
	if (!CachedPlayerController)
	{
		CachedPlayerController = Cast<AFPSPlayerController>(GetController());
	}

	// Setup hands location (LOCAL operation for locally controlled player)
	// Arms mesh is OnlyOwnerSee, so this only affects local player
	SetupHandsLocation();

	// NOTE: LinkDefaultLayer() is already called in BeginPlay() on ALL machines
	// No need to call it again here (would be redundant)

	if (CachedPlayerController)
	{
		CachedPlayerController->SetupInputMapping();
		CachedPlayerController->SetViewTarget(this);
	}
}

void AFPSCharacter::UpdatePitch(float Y)
{
	float NewPitch = (Y * -1.0f) + Pitch;
	NewPitch = FMath::ClampAngle(NewPitch, -45.0f, 45.0f);

	if (IsLocallyControlled())
	{
		Pitch = NewPitch;
		UpdateSpineRotations();

		if (!HasAuthority())
		{
			Server_UpdatePitch(Pitch);
		}
	}
}

void AFPSCharacter::Server_UpdatePitch_Implementation(float NewPitch)
{
	Pitch = FMath::ClampAngle(NewPitch, -45.0f, 45.0f);
	UpdateSpineRotations();
}

void AFPSCharacter::UpdateSpineRotations()
{
	// Distribute pitch across spine bones (40%, 50%, 60%, 20%)
	Spine_03->SetRelativeRotation(FRotator(Pitch * 0.4f, 90.0f, 0.0f));
	Spine_04->SetRelativeRotation(FRotator(Pitch * 0.5f, 0.0f, 0.0f));
	Spine_05->SetRelativeRotation(FRotator(Pitch * 0.6f, 0.0f, 0.0f));
	Neck_01->SetRelativeRotation(FRotator(Pitch * 0.2f, 0.0f, 0.0f));
}

void AFPSCharacter::SetupHandsLocation()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (ActiveItem && ActiveItem->Implements<UHoldableInterface>())
	{
		HandsOffset = IHoldableInterface::Execute_GetHandsOffset(ActiveItem);
	}
	else
	{
		HandsOffset = DefaultHandsOffset;
	}

	Arms->SetRelativeLocation(HandsOffset);
}

void AFPSCharacter::LinkDefaultLayer()
{
	if (!DefaultAnimLayer)
	{
		return;
	}

	GetMesh()->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
	Legs->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
	Arms->GetAnimInstance()->LinkAnimClassLayers(DefaultAnimLayer);
}

void AFPSCharacter::UnlinkDefaultLayer()
{
	if (!DefaultAnimLayer)
	{
		return;
	}

	GetMesh()->GetAnimInstance()->UnlinkAnimClassLayers(DefaultAnimLayer);
	Legs->GetAnimInstance()->UnlinkAnimClassLayers(DefaultAnimLayer);
	Arms->GetAnimInstance()->UnlinkAnimClassLayers(DefaultAnimLayer);
}

void AFPSCharacter::LookYaw(const FInputActionValue& Value)
{
	if (bIsDeath)
	{
		return;
	}

	float YawValue = Value.Get<float>();
	float Sensitivity = bIsAiming ? 0.25f : 0.5f;

	AddControllerYawInput(YawValue * Sensitivity);
}

void AFPSCharacter::LookPitch(const FInputActionValue& Value)
{
	if (bIsDeath)
	{
		return;
	}

	float PitchValue = Value.Get<float>();
	float Sensitivity = bIsAiming ? 0.25f : 0.5f;

	UpdatePitch(PitchValue * Sensitivity);
}

void AFPSCharacter::Move(const FInputActionValue& Value)
{
	if (bIsDeath)
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

	if (CurrentMovementMode == EFPSMovementMode::Sprint && CMC)
	{
		float SprintMultiplier = GetSprintDirectionMultiplier(CurrentMovementVector);
		float ClampedSpeed = FMath::Lerp(JogSpeed, SprintSpeed, SprintMultiplier);
		CMC->MaxWalkSpeed = ClampedSpeed;
	}

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

	// Create interaction context
	FInteractionContext Ctx;
	Ctx.Controller = GetController();
	Ctx.Pawn = this;
	Ctx.Instigator = this;

	// Get available verbs
	TArray<FGameplayTag> Verbs;
	Interactable->Execute_GetVerbs(LastInteractableActor, Verbs, Ctx);

	if (Verbs.Num() > 0)
	{
		const FGameplayTag& FirstVerb = Verbs[0];

		// Re-check CanInteract in case state changed between frames
		if (Interactable->Execute_CanInteract(LastInteractableActor, FirstVerb, Ctx))
		{
			// Execute the interaction
			Interactable->Execute_Interact(LastInteractableActor, FirstVerb, Ctx);

			// Debug feedback
			FText InteractionText = Interactable->Execute_GetInteractionText(LastInteractableActor, FirstVerb, Ctx);
			UE_LOG(LogTemp, Warning, TEXT("✓ Interacted: %s"), *InteractionText.ToString());

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
					FString::Printf(TEXT("✓ Interacted: %s"), *InteractionText.ToString()));
			}
		}
	}
}

void AFPSCharacter::DropPressed()
{
	// Only locally controlled players can drop items
	if (!IsLocallyControlled())
	{
		return;
	}

	// Check if we have an active item to drop
	if (!ActiveItem)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
				TEXT("⚠ No active item to drop"));
		}
		return;
	}

	// Call Server RPC to drop the active item
	Server_DropItem(ActiveItem);

	// Debug feedback
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
			FString::Printf(TEXT("✓ Dropping item: %s"), *ActiveItem->GetName()));
	}
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
	if (!IsLocallyControlled())
	{
		UpdateSpineRotations();
	}
}

void AFPSCharacter::OnRep_ActiveItem()
{
	// OnRep runs on CLIENTS when ActiveItem replicates from server
	// Execute LOCAL setup on clients (server already executed it in EquipItem)
	SetupActiveItemLocal();
}

void AFPSCharacter::SetupActiveItemLocal()
{
	// ============================================
	// LOCAL SETUP (runs on ALL machines)
	// ============================================
	// Called from:
	// - EquipItem() on SERVER
	// - OnRep_ActiveItem() on CLIENTS
	// This follows MULTIPLAYER_GUIDELINES.md OnRep pattern

	FString MachineRole = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
	UE_LOG(LogTemp, Warning, TEXT("[%s] SetupActiveItemLocal() - Item: %s"), *MachineRole, ActiveItem ? *ActiveItem->GetName() : TEXT("NULL"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("[%s] SetupActiveItemLocal() - Item: %s"), *MachineRole, ActiveItem ? *ActiveItem->GetName() : TEXT("NULL")));
	}

	// Case 1: No active item (unarmed state)
	if (!ActiveItem)
	{
		// Clear hands offset
		HandsOffset = DefaultHandsOffset;
		if (Arms)
		{
			Arms->SetRelativeLocation(HandsOffset);
		}

		// Link unarmed animation layer (use helper function)
		LinkDefaultLayer();

		// Update HUD (clear weapon display) - ONLY for locally controlled player
		if (IsLocallyControlled() && CachedPlayerController && CachedPlayerController->Implements<UPlayerHUDInterface>())
		{
			IPlayerHUDInterface::Execute_UpdateActiveWeapon(CachedPlayerController, nullptr);
		}

		return;
	}

	// Case 2: Item equipped
	if (!ActiveItem->Implements<UHoldableInterface>())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] ActiveItem does not implement IHoldableInterface!"), *MachineRole);
		return;
	}

	// 1. PREPARE ITEM FOR EQUIP
	// Ensure physics and collision are disabled (should already be done in PickupItem, but verify)
	ActiveItem->SetActorEnableCollision(false);

	// Disable physics and collision on all mesh components
	TArray<UMeshComponent*> MeshComponents;
	ActiveItem->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(MeshComp))
		{
			PrimComp->SetSimulatePhysics(false);
			PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// Show item
	ActiveItem->SetActorHiddenInGame(false);

	// 2. MESH ATTACHMENT (via IHoldableInterface)
	UMeshComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(ActiveItem);
	UMeshComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(ActiveItem);
	FName AttachSocket = IHoldableInterface::Execute_GetAttachSocket(ActiveItem);

	// Check if this is locally controlled player
	bool bIsLocalPlayer = IsLocallyControlled();

	// Attach FPS mesh to Arms (ONLY for locally controlled player)
	if (bIsLocalPlayer && FPSMeshComp && Arms)
	{
		// Attach to socket
		FPSMeshComp->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);

		// Set relative transform to zero (snap exactly to socket)
		FPSMeshComp->SetRelativeLocation(FVector::ZeroVector);
		FPSMeshComp->SetRelativeRotation(FRotator::ZeroRotator);
		FPSMeshComp->SetRelativeScale3D(FVector::OneVector);

		UE_LOG(LogTemp, Log, TEXT("✓ [%s-LOCAL] Attached FPS mesh to Arms socket: %s"), *MachineRole, *AttachSocket.ToString());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
				FString::Printf(TEXT("✓ [%s-LOCAL] Attached FPS mesh to Arms socket: %s"), *MachineRole, *AttachSocket.ToString()));
		}
	}

	// Attach TPS mesh to Body (for ALL machines)
	if (TPSMeshComp && GetMesh())
	{
		// Attach to socket
		TPSMeshComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);

		// Set relative transform to zero (snap exactly to socket)
		TPSMeshComp->SetRelativeLocation(FVector::ZeroVector);
		TPSMeshComp->SetRelativeRotation(FRotator::ZeroRotator);
		TPSMeshComp->SetRelativeScale3D(FVector::OneVector);

		UE_LOG(LogTemp, Log, TEXT("✓ [%s] Attached TPS mesh to Body socket: %s"), *MachineRole, *AttachSocket.ToString());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
				FString::Printf(TEXT("✓ [%s] Attached TPS mesh to Body socket: %s"), *MachineRole, *AttachSocket.ToString()));
		}
	}

	// Fallback: If no separate mesh components, attach actor to body
	if (!FPSMeshComp && !TPSMeshComp)
	{
		ActiveItem->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
		ActiveItem->SetActorRelativeTransform(FTransform::Identity);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
				FString::Printf(TEXT("⚠ [%s] No FPS/TPS meshes - attached actor to body"), *MachineRole));
		}
	}

	// 3. ANIMATION LINKING (via IHoldableInterface)
	TSubclassOf<UAnimInstance> ItemAnimLayer = IHoldableInterface::Execute_GetAnimLayer(ActiveItem);
	if (ItemAnimLayer)
	{
		// Unlink default layer first (use helper function)
		UnlinkDefaultLayer();

		// Link item-specific animation layer
		GetMesh()->GetAnimInstance()->LinkAnimClassLayers(ItemAnimLayer);
		Legs->GetAnimInstance()->LinkAnimClassLayers(ItemAnimLayer);
		Arms->GetAnimInstance()->LinkAnimClassLayers(ItemAnimLayer);

		UE_LOG(LogTemp, Warning, TEXT("✓ [%s] Linked animation layer: %s"), *MachineRole, *ItemAnimLayer->GetName());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
				FString::Printf(TEXT("✓ [%s] Linked animation layer: %s"), *MachineRole, *ItemAnimLayer->GetName()));
		}
	}

	// 4. HANDS OFFSET UPDATE (via IHoldableInterface)
	SetupHandsLocation();

	// 5. HUD UPDATE (via PlayerHUDInterface) - ONLY for locally controlled player
	if (IsLocallyControlled() && CachedPlayerController && CachedPlayerController->Implements<UPlayerHUDInterface>())
	{
		// Update active weapon display
		IPlayerHUDInterface::Execute_UpdateActiveWeapon(CachedPlayerController, ActiveItem);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
				TEXT("✓ Updated HUD with active item"));
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("✓ [%s] Setup complete: %s"), *MachineRole, *ActiveItem->GetName()));
	}
}

void AFPSCharacter::OnRep_IsDeath()
{
	if (bIsDeath)
	{
		// Death logic - can be extended in Blueprint
	}
}

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
	// Early exit if no camera
	if (!Camera)
	{
		return;
	}

	// Lazy initialization of cached player controller (for client)
	if (!CachedPlayerController)
	{
		CachedPlayerController = Cast<AFPSPlayerController>(GetController());
		if (!CachedPlayerController)
		{
			return;
		}

		// Debug: Log when we cache the controller
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, TEXT("PlayerController cached!"));
		}
	}

	// Get camera location and direction
	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector CameraDirection = Camera->GetForwardVector();

	// Start trace 50cm forward to avoid hitting own character
	const FVector TraceStart = CameraLocation + CameraDirection * 50.0f;
	const FVector TraceEnd = CameraLocation + CameraDirection * InteractionDistance;

	// Perform line trace
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

	// Debug: Check if we're actually tracing
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::White, FString::Printf(TEXT("Trace: %s"), bHit ? TEXT("HIT") : TEXT("MISS")));
	}

	// Debug: Draw trace line (from actual trace start point)
	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHit ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);

	// Debug: Draw hit point
	if (bHit)
	{
		DrawDebugSphere(GetWorld(), Hit.Location, 10.0f, 8, FColor::Yellow, false, 0.0f, 0, 2.0f);

		// Log what we hit
		if (GEngine)
		{
			FString HitActorName = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("NULL");
			FString HitComponentName = Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT("NULL");
			GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Cyan, FString::Printf(TEXT("Hit: %s | Component: %s"), *HitActorName, *HitComponentName));
		}
	}

	// Check if we hit an interactable object
	if (bHit && Hit.GetActor() && Hit.GetActor()->Implements<UInteractableInterface>())
	{
		AActor* HitActor = Hit.GetActor();
		IInteractableInterface* Interactable = Cast<IInteractableInterface>(HitActor);

		// Create interaction context
		FInteractionContext Ctx;
		Ctx.Controller = GetController();
		Ctx.Pawn = this;
		Ctx.Instigator = this;
		Ctx.Hit = Hit;

		// Get available verbs
		TArray<FGameplayTag> Verbs;
		Interactable->Execute_GetVerbs(HitActor, Verbs, Ctx);

		if (Verbs.Num() > 0)
		{
			const FGameplayTag& FirstVerb = Verbs[0];

			// Check if interaction is allowed
			if (Interactable->Execute_CanInteract(HitActor, FirstVerb, Ctx))
			{
				// Get interaction text
				FText InteractionText = Interactable->Execute_GetInteractionText(HitActor, FirstVerb, Ctx);

				// Debug: Screen message
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Green, FString::Printf(TEXT("Interaction: %s"), *InteractionText.ToString()));
				}

				// Update PlayerController via PlayerHUDInterface
				if (CachedPlayerController->Implements<UPlayerHUDInterface>())
				{
					IPlayerHUDInterface::Execute_UpdateItemInfo(CachedPlayerController, InteractionText.ToString());
				}

				LastInteractableActor = HitActor;
				return;
			}
		}
	}

	// No valid interactable found - clear HUD if we were looking at something before
	if (LastInteractableActor != nullptr)
	{
		if (CachedPlayerController->Implements<UPlayerHUDInterface>())
		{
			IPlayerHUDInterface::Execute_UpdateItemInfo(CachedPlayerController, FString());
		}

		// Debug: Clear screen message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Red, TEXT(""));
		}

		LastInteractableActor = nullptr;
	}
}

// ============================================
// INVENTORY SYSTEM IMPLEMENTATION
// ============================================

void AFPSCharacter::Server_PickupItem_Implementation(AActor* Item)
{
	// Validation
	if (!Item || !HasAuthority())
	{
		return;
	}

	// Verify item is pickupable
	if (!Item->Implements<UPickupableInterface>())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("❌ Item %s is not pickupable!"), *Item->GetName()));
		}
		return;
	}

	// Verify item can be picked (interface validation)
	FInteractionContext Ctx;
	Ctx.Controller = GetController();
	Ctx.Pawn = this;
	Ctx.Instigator = this;

	if (!IPickupableInterface::Execute_CanBePicked(Item, Ctx))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ Cannot pick %s (CanBePicked = false)"), *Item->GetName()));
		}
		return;
	}

	// Execute pickup
	PickupItem(Item);
}

void AFPSCharacter::PickupItem(AActor* Item)
{
	if (!Item || !HasAuthority() || !InventoryComp)
	{
		return;
	}

	// Debug: Log pickup start
	UE_LOG(LogTemp, Warning, TEXT("FPSCharacter::PickupItem() - Picking up: %s"), *Item->GetName());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("FPSCharacter::PickupItem() - Picking up: %s"), *Item->GetName()));
	}

	// GENERIC PICKUP LOGIC (applies to ALL items)

	// 1. Set owner
	Item->SetOwner(this);

	// 2. Disable physics and collision on item actor
	Item->SetActorEnableCollision(false);

	// 3. Disable physics and collision on ALL mesh components (including child actors like Magazines)
	TArray<UMeshComponent*> MeshComponents;
	Item->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(MeshComp))
		{
			PrimComp->SetSimulatePhysics(false);
			PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			UE_LOG(LogTemp, Log, TEXT("✓ Disabled collision on: %s (Owner: %s)"),
				*MeshComp->GetName(),
				MeshComp->GetOwner() ? *MeshComp->GetOwner()->GetName() : TEXT("NULL"));

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
					FString::Printf(TEXT("✓ Disabled collision on: %s (Owner: %s)"),
						*MeshComp->GetName(),
						MeshComp->GetOwner() ? *MeshComp->GetOwner()->GetName() : TEXT("NULL")));
			}
		}
	}

	// 4. Hide from world (will be shown when equipped)
	// NOTE: Do NOT attach item actor to character here - meshes will be attached in SetupActiveItemLocal()
	Item->SetActorHiddenInGame(true);

	// 5. Add to inventory via component (handles validation + replication + events)
	if (InventoryComp->AddItem(Item))
	{
		// 6. Notify item via PickupableInterface (item-specific behavior)
		FInteractionContext Ctx;
		Ctx.Controller = GetController();
		Ctx.Pawn = this;
		Ctx.Instigator = this;

		IPickupableInterface::Execute_OnPicked(Item, this, Ctx);

		// Debug: Log inventory state
		UE_LOG(LogTemp, Warning, TEXT("✓ Pickup complete - Inventory size: %d"), InventoryComp->GetItemCount());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				FString::Printf(TEXT("✓ Pickup complete - Inventory size: %d"), InventoryComp->GetItemCount()));
		}

		// 7. Auto-equip if this is first item and it's holdable
		if (InventoryComp->GetItemCount() == 1 && Item->Implements<UHoldableInterface>())
		{
			EquipItem(Item);
		}
	}
	else
	{
		// Failed to add to inventory - revert changes
		Item->SetOwner(nullptr);
		Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Item->SetActorHiddenInGame(false);
	}
}

void AFPSCharacter::EquipItem(AActor* Item)
{
	if (!Item || !HasAuthority() || !InventoryComp)
	{
		return;
	}

	// Verify item is in inventory
	if (!InventoryComp->ContainsItem(Item))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("❌ Cannot equip %s - not in inventory!"), *Item->GetName()));
		}
		return;
	}

	// Verify item is holdable
	if (!Item->Implements<UHoldableInterface>())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ Cannot equip %s - not holdable!"), *Item->GetName()));
		}
		return;
	}

	// Debug: Log equip start (SERVER)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Magenta,
			FString::Printf(TEXT("[SERVER] FPSCharacter::EquipItem() - %s"), *Item->GetName()));
	}

	// Unequip current item first
	if (ActiveItem != nullptr)
	{
		UnequipCurrentItem();
	}

	// ============================================
	// SERVER-SIDE LOGIC
	// ============================================

	// 1. Set as active item (REPLICATED - triggers OnRep_ActiveItem on clients)
	ActiveItem = Item;

	// 2. Set owner (for replication and authority)
	Item->SetOwner(this);

	// 3. Notify item via HoldableInterface (SERVER-side item-specific behavior)
	IHoldableInterface::Execute_OnEquipped(Item, this);

	// 4. Execute LOCAL setup on server (same as clients will do via OnRep_ActiveItem)
	// This follows MULTIPLAYER_GUIDELINES.md OnRep pattern:
	// "Server and client must execute SAME local logic"
	SetupActiveItemLocal();

	// Debug: Confirm server equip
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("✓ [SERVER] Equipped: %s (ActiveItem replicated to clients)"), *Item->GetName()));
	}
}

void AFPSCharacter::UnequipCurrentItem()
{
	if (!ActiveItem || !HasAuthority())
	{
		return;
	}

	// Debug: Log unequip (SERVER)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Purple,
			FString::Printf(TEXT("[SERVER] UnequipCurrentItem() - %s"), *ActiveItem->GetName()));
	}

	// ============================================
	// SERVER-SIDE LOGIC ONLY
	// ============================================

	// 1. Notify item via HoldableInterface (SERVER-side item-specific behavior)
	if (ActiveItem->Implements<UHoldableInterface>())
	{
		IHoldableInterface::Execute_OnUnequipped(ActiveItem);
	}

	// 2. Hide item
	ActiveItem->SetActorHiddenInGame(true);

	// 3. Clear active item reference (REPLICATED - triggers OnRep_ActiveItem on clients)
	ActiveItem = nullptr;

	// 4. Execute LOCAL setup on server (same as clients will do via OnRep_ActiveItem)
	// This follows MULTIPLAYER_GUIDELINES.md OnRep pattern:
	// "Server and client must execute SAME local logic"
	// SetupActiveItemLocal() handles nullptr case (unarmed state - restores DefaultAnimLayer)
	SetupActiveItemLocal();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			TEXT("✓ [SERVER] Unequipped (ActiveItem cleared, replicated to clients)"));
	}
}

void AFPSCharacter::Server_SwitchToItemAtIndex_Implementation(int32 Index)
{
	if (!HasAuthority() || !InventoryComp)
	{
		return;
	}

	SwitchToItemAtIndex(Index);
}

void AFPSCharacter::SwitchToItemAtIndex(int32 Index)
{
	if (!InventoryComp)
	{
		return;
	}

	// Get item at index
	AActor* Item = InventoryComp->GetItemAtIndex(Index);

	if (!Item)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				FString::Printf(TEXT("⚠ No item at index %d"), Index));
		}
		return;
	}

	// Check if item is already equipped
	if (ActiveItem == Item)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
				FString::Printf(TEXT("Item already equipped: %s"), *Item->GetName()));
		}
		return;
	}

	// Equip the item
	EquipItem(Item);
}

void AFPSCharacter::Server_DropItem_Implementation(AActor* Item)
{
	if (!Item || !HasAuthority() || !InventoryComp)
	{
		return;
	}

	// Verify item is in inventory
	if (!InventoryComp->ContainsItem(Item))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("❌ Cannot drop %s - not in inventory!"), *Item->GetName()));
		}
		return;
	}

	DropItem(Item);
}

void AFPSCharacter::DropItem(AActor* Item)
{
	if (!Item || !HasAuthority() || !InventoryComp)
	{
		return;
	}

	// Debug: Log drop
	UE_LOG(LogTemp, Warning, TEXT("FPSCharacter::DropItem() - Dropping: %s"), *Item->GetName());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
			FString::Printf(TEXT("FPSCharacter::DropItem() - Dropping: %s"), *Item->GetName()));
	}

	// Unequip if it's the active item
	if (ActiveItem == Item)
	{
		UnequipCurrentItem();
	}

	// GENERIC DROP LOGIC (applies to ALL items)

	// 1. Remove from inventory via component (handles replication + events)
	if (InventoryComp->RemoveItem(Item))
	{
		// 2. Clear owner
		Item->SetOwner(nullptr);

		// 3. Detach meshes from character (if item has HoldableInterface)
		if (Item->Implements<UHoldableInterface>())
		{
			UMeshComponent* FPSMeshComp = IHoldableInterface::Execute_GetFPSMeshComponent(Item);
			UMeshComponent* TPSMeshComp = IHoldableInterface::Execute_GetTPSMeshComponent(Item);

			if (FPSMeshComp)
			{
				FPSMeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			if (TPSMeshComp)
			{
				TPSMeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
		}
		else
		{
			// Fallback: detach actor if no meshes
			Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}

		// 4. Enable collision on item actor
		Item->SetActorEnableCollision(true);

		// 5. Enable physics and collision on ALL mesh components (including child actors like Magazines)
		TArray<UMeshComponent*> MeshComponents;
		Item->GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* MeshComp : MeshComponents)
		{
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(MeshComp))
			{
				PrimComp->SetSimulatePhysics(true);
				PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

				UE_LOG(LogTemp, Log, TEXT("✓ Enabled collision on: %s (Owner: %s)"),
					*MeshComp->GetName(),
					MeshComp->GetOwner() ? *MeshComp->GetOwner()->GetName() : TEXT("NULL"));

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
						FString::Printf(TEXT("✓ Enabled collision on: %s (Owner: %s)"),
							*MeshComp->GetName(),
							MeshComp->GetOwner() ? *MeshComp->GetOwner()->GetName() : TEXT("NULL")));
				}
			}
		}

		// 6. Show in world
		Item->SetActorHiddenInGame(false);

		// 7. Spawn item in front of player
		FVector ForwardOffset = GetActorForwardVector() * 100.0f;
		Item->SetActorLocation(GetActorLocation() + ForwardOffset + FVector(0, 0, 50.0f));

		// 7. Notify item via PickupableInterface (item-specific behavior)
		if (Item->Implements<UPickupableInterface>())
		{
			FInteractionContext Ctx;
			Ctx.Controller = GetController();
			Ctx.Pawn = this;
			Ctx.Instigator = this;

			IPickupableInterface::Execute_OnDropped(Item, Ctx);
		}

		// Debug: Confirm drop
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				FString::Printf(TEXT("✓ Dropped: %s | Inventory size: %d"), *Item->GetName(), InventoryComp->GetItemCount()));
		}
	}
}

