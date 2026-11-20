#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/VelrathCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathCameraDefaultsTest,
    "VELRATH.Camera.DefaultParameters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathCameraDefaultsTest::RunTest(const FString& Parameters)
{
    const AVelrathCharacter* Character = GetDefault<AVelrathCharacter>();
    TestNotNull(TEXT("The VELRATH Character default object exists"), Character);
    if (Character == nullptr)
    {
        return false;
    }

    const USpringArmComponent* CameraBoom = Character->GetCameraBoom();
    const UCameraComponent* FollowCamera = Character->GetFollowCamera();
    const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();

    TestNotNull(TEXT("Spring arm exists"), CameraBoom);
    TestNotNull(TEXT("Follow camera exists"), FollowCamera);
    TestNotNull(TEXT("CharacterMovementComponent exists"), MovementComponent);
    if (CameraBoom == nullptr || FollowCamera == nullptr || MovementComponent == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("Target arm length is 700 cm"), CameraBoom->TargetArmLength, 700.0f);
    TestTrue(TEXT("Camera socket offset is elevated by 100 cm"), CameraBoom->SocketOffset.Equals(FVector(0.0f, 0.0f, 100.0f)));
    TestEqual(TEXT("Initial controller pitch is -20 degrees"), Character->GetInitialCameraPitch(), -20.0f);
    TestEqual(TEXT("Camera field of view is 80 degrees"), FollowCamera->FieldOfView, 80.0f);

    TestTrue(TEXT("Spring arm follows Pawn ControlRotation"), CameraBoom->bUsePawnControlRotation);
    TestFalse(TEXT("Camera does not apply Pawn ControlRotation a second time"), FollowCamera->bUsePawnControlRotation);
    TestTrue(TEXT("Spring arm collision test is enabled"), CameraBoom->bDoCollisionTest);
    TestFalse(TEXT("Camera lag is disabled by default"), CameraBoom->bEnableCameraLag);
    TestFalse(TEXT("Spring arm is not replicated"), CameraBoom->GetIsReplicated());
    TestFalse(TEXT("Camera is not replicated"), FollowCamera->GetIsReplicated());

    TestTrue(TEXT("Spring arm is attached to the Character root"), CameraBoom->GetAttachParent() == Character->GetRootComponent());
    TestTrue(TEXT("Camera is attached to the spring arm"), FollowCamera->GetAttachParent() == CameraBoom);
    TestEqual(TEXT("Camera uses the spring arm socket"), FollowCamera->GetAttachSocketName(), USpringArmComponent::SocketName);

    TestFalse(TEXT("Character pitch does not follow ControlRotation"), Character->bUseControllerRotationPitch);
    TestFalse(TEXT("Character yaw does not follow ControlRotation"), Character->bUseControllerRotationYaw);
    TestFalse(TEXT("Character roll does not follow ControlRotation"), Character->bUseControllerRotationRoll);
    TestTrue(TEXT("Character remains oriented to camera-relative movement"), MovementComponent->bOrientRotationToMovement);

    return true;
}

#endif
