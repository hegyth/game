#include "Player/VelrathCharacter.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

AVelrathCharacter::AVelrathCharacter()
{
    bReplicates = true;

    GetCapsuleComponent()->InitCapsuleSize(38.0f, 90.0f);

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    MovementComponent->MaxWalkSpeed = WalkSpeed;
    MovementComponent->MaxAcceleration = 3000.0f;
    MovementComponent->BrakingDecelerationWalking = 3600.0f;
    MovementComponent->bOrientRotationToMovement = true;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 700.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 100.0f);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->bEnableCameraLag = false;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->FieldOfView = 80.0f;
    FollowCamera->bUsePawnControlRotation = false;

    DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DefaultMappingContext"));

    MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveAction"));
    MoveAction->ValueType = EInputActionValueType::Axis2D;

    LookAction = CreateDefaultSubobject<UInputAction>(TEXT("LookAction"));
    LookAction->ValueType = EInputActionValueType::Axis2D;

    SprintAction = CreateDefaultSubobject<UInputAction>(TEXT("SprintAction"));
    SprintAction->ValueType = EInputActionValueType::Boolean;

    UInputModifierSwizzleAxis* ForwardSwizzle =
        CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveForwardSwizzle"));
    UInputModifierNegate* BackwardNegate =
        CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveBackwardNegate"));
    UInputModifierSwizzleAxis* BackwardSwizzle =
        CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveBackwardSwizzle"));
    UInputModifierNegate* LeftNegate =
        CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveLeftNegate"));

    DefaultMappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(ForwardSwizzle);
    FEnhancedActionKeyMapping& BackwardMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
    BackwardMapping.Modifiers.Add(BackwardNegate);
    BackwardMapping.Modifiers.Add(BackwardSwizzle);
    DefaultMappingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(LeftNegate);
    DefaultMappingContext->MapKey(MoveAction, EKeys::D);
    DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
    DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // Keep the native subobject name stable so existing Character Blueprints retain their serialized reference.
    AttributeSet = CreateDefaultSubobject<UVelrathAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AVelrathCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AVelrathCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    ApplyDefaultMappingContext();

    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (PlayerController != nullptr && PlayerController->IsLocalController())
    {
        FRotator ControlRotation = PlayerController->GetControlRotation();
        ControlRotation.Pitch = InitialCameraPitch;
        ControlRotation.Roll = 0.0f;
        PlayerController->SetControlRotation(ControlRotation);
    }
}

void AVelrathCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
    EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
    EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
    EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::StartSprinting);
    EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::StopSprinting);
    EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::StopSprinting);
}

void AVelrathCharacter::ApplyDefaultMappingContext()
{
    const APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (PlayerController == nullptr || !PlayerController->IsLocalController())
    {
        return;
    }

    const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (InputSubsystem != nullptr && DefaultMappingContext != nullptr &&
        !InputSubsystem->HasMappingContext(DefaultMappingContext))
    {
        InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
    }
}

void AVelrathCharacter::Move(const FInputActionValue& Value)
{
    if (Controller == nullptr)
    {
        return;
    }

    const FVector2D MovementInput = Value.Get<FVector2D>();
    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDirection, MovementInput.Y);
    AddMovementInput(RightDirection, MovementInput.X);
}

void AVelrathCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookInput = Value.Get<FVector2D>();
    AddControllerYawInput(LookInput.X);
    AddControllerPitchInput(LookInput.Y);
}

void AVelrathCharacter::StartSprinting()
{
    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AVelrathCharacter::StopSprinting()
{
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AVelrathCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
    InitializeDefaultAttributes();
}

void AVelrathCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilityActorInfo();
}

void AVelrathCharacter::InitializeAbilityActorInfo()
{
    AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void AVelrathCharacter::InitializeDefaultAttributes()
{
    if (!HasAuthority() || bDefaultAttributesInitialized || AbilitySystemComponent == nullptr || AttributeSet == nullptr)
    {
        return;
    }

    AbilitySystemComponent->SetNumericAttributeBase(
        UVelrathAttributeSet::GetMaxHealthAttribute(),
        UVelrathAttributeSet::DefaultMaxHealth);
    AbilitySystemComponent->SetNumericAttributeBase(
        UVelrathAttributeSet::GetHealthAttribute(),
        UVelrathAttributeSet::DefaultHealth);
    AbilitySystemComponent->SetNumericAttributeBase(
        UVelrathAttributeSet::GetMaxStaminaAttribute(),
        UVelrathAttributeSet::DefaultMaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(
        UVelrathAttributeSet::GetStaminaAttribute(),
        UVelrathAttributeSet::DefaultStamina);

    bDefaultAttributesInitialized = true;
}

