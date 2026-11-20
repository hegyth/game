#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/VelrathCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathMovementDefaultsTest,
    "VELRATH.Movement.DefaultParameters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathMovementDefaultsTest::RunTest(const FString& Parameters)
{
    const AVelrathCharacter* Character = GetDefault<AVelrathCharacter>();
    TestNotNull(TEXT("The VELRATH Character default object exists"), Character);
    if (Character == nullptr)
    {
        return false;
    }

    const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
    const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent();

    TestNotNull(TEXT("CharacterMovementComponent exists"), MovementComponent);
    TestNotNull(TEXT("CapsuleComponent exists"), CapsuleComponent);
    if (MovementComponent == nullptr || CapsuleComponent == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("Walk speed is 520 cm/s"), MovementComponent->MaxWalkSpeed, 520.0f);
    TestEqual(TEXT("Sprint speed is stored for the future Sprint task"), Character->GetSprintSpeed(), 660.0f);
    TestEqual(TEXT("Maximum acceleration is 3000 cm/s^2"), MovementComponent->MaxAcceleration, 3000.0f);
    TestEqual(TEXT("Walking braking deceleration is 3600 cm/s^2"), MovementComponent->BrakingDecelerationWalking, 3600.0f);
    TestEqual(TEXT("Backward movement multiplier is stored"), Character->GetBackwardMovementMultiplier(), 0.85f);
    TestEqual(TEXT("Attack movement multiplier is stored"), Character->GetAttackMovementMultiplier(), 0.45f);
    TestEqual(TEXT("Block movement multiplier is stored"), Character->GetBlockMovementMultiplier(), 0.60f);
    TestEqual(TEXT("Capsule radius is 38 cm"), CapsuleComponent->GetUnscaledCapsuleRadius(), 38.0f);
    TestEqual(TEXT("Capsule half-height is 90 cm"), CapsuleComponent->GetUnscaledCapsuleHalfHeight(), 90.0f);
    TestTrue(TEXT("Character replication remains enabled"), Character->GetIsReplicated());

    return true;
}

#endif
