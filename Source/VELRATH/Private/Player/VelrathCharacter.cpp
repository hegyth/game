#include "Player/VelrathCharacter.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

AVelrathCharacter::AVelrathCharacter()
{
    bReplicates = true;

    GetCapsuleComponent()->InitCapsuleSize(38.0f, 90.0f);

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    MovementComponent->MaxWalkSpeed = 520.0f;
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

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AVelrathCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AVelrathCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (PlayerController != nullptr && PlayerController->IsLocalController())
    {
        FRotator ControlRotation = PlayerController->GetControlRotation();
        ControlRotation.Pitch = InitialCameraPitch;
        ControlRotation.Roll = 0.0f;
        PlayerController->SetControlRotation(ControlRotation);
    }
}

void AVelrathCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
}

void AVelrathCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilityActorInfo();
}

void AVelrathCharacter::InitializeAbilityActorInfo()
{
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

