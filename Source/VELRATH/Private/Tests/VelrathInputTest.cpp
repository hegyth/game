#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Player/VelrathCharacter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
const FEnhancedActionKeyMapping* FindMapping(
    const UInputMappingContext& MappingContext,
    const UInputAction* Action,
    const FKey Key)
{
    return MappingContext.GetMappings().FindByPredicate(
        [Action, Key](const FEnhancedActionKeyMapping& Mapping)
        {
            return Mapping.Action == Action && Mapping.Key == Key;
        });
}

const FEnhancedInputActionEventBinding* FindBinding(
    const UEnhancedInputComponent& InputComponent,
    const UInputAction* Action,
    const ETriggerEvent TriggerEvent)
{
    for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : InputComponent.GetActionEventBindings())
    {
        if (Binding->GetAction() == Action && Binding->GetTriggerEvent() == TriggerEvent)
        {
            return Binding.Get();
        }
    }

    return nullptr;
}

class FValidateVelrathInputPieCommand final : public IAutomationLatentCommand
{
public:
    explicit FValidateVelrathInputPieCommand(FAutomationTestBase* InTest)
        : Test(InTest)
        , StartTimeSeconds(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        if (GEngine == nullptr)
        {
            Test->AddError(TEXT("GEngine is unavailable while waiting for input PIE."));
            return true;
        }

        UWorld* PieWorld = nullptr;
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World() != nullptr)
            {
                PieWorld = WorldContext.World();
                break;
            }
        }

        APlayerController* PlayerController = PieWorld != nullptr ? PieWorld->GetFirstPlayerController() : nullptr;
        AVelrathCharacter* Character =
            PlayerController != nullptr ? Cast<AVelrathCharacter>(PlayerController->GetPawn()) : nullptr;
        if (Character == nullptr)
        {
            if (FPlatformTime::Seconds() - StartTimeSeconds >= 15.0)
            {
                Test->AddError(TEXT("Timed out waiting for BP_VelrathCharacter in input PIE."));
                return true;
            }

            return false;
        }

        ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
        UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
            LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
        if (InputSubsystem == nullptr)
        {
            Test->AddError(TEXT("The local PlayerController has no Enhanced Input subsystem."));
            return true;
        }

        if (!bInitialized)
        {
            Test->TestEqual(
                TEXT("Input PIE uses BP_VelrathCharacter"),
                Character->GetClass()->GetPathName(),
                FString(TEXT("/Game/Characters/BP_VelrathCharacter.BP_VelrathCharacter_C")));
            Test->TestTrue(
                TEXT("The local PlayerController has the Character mapping context"),
                InputSubsystem->HasMappingContext(Character->GetDefaultMappingContext()));

            const UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(Character->InputComponent);
            Test->TestNotNull(TEXT("BP_VelrathCharacter uses UEnhancedInputComponent"), InputComponent);
            Test->TestEqual(
                TEXT("BP_VelrathCharacter installs all five input event bindings"),
                InputComponent != nullptr ? InputComponent->GetActionEventBindings().Num() : 0,
                5);

            const UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
            Test->TestNotNull(TEXT("BP_VelrathCharacter has an active AnimInstance"), AnimInstance);
            if (AnimInstance != nullptr)
            {
                Test->TestEqual(
                    TEXT("BP_VelrathCharacter uses ABP_Unarmed"),
                    AnimInstance->GetClass()->GetPathName(),
                    FString(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
            }

            InitialLocation = Character->GetActorLocation();
            InitialControlYaw = PlayerController->GetControlRotation().Yaw;
            ExerciseStartTimeSeconds = FPlatformTime::Seconds();
            bInitialized = true;
        }

        if (!bReleasedInput)
        {
            InputSubsystem->InjectInputForAction(
                Character->GetMoveAction(),
                FInputActionValue(FVector2D(0.0f, 1.0f)),
                {},
                {});
            InputSubsystem->InjectInputForAction(
                Character->GetLookAction(),
                FInputActionValue(FVector2D(0.5f, 0.0f)),
                {},
                {});
            InputSubsystem->InjectInputForAction(
                Character->GetSprintAction(),
                FInputActionValue(true),
                {},
                {});

            bObservedSprintSpeed |= FMath::IsNearlyEqual(
                Character->GetCharacterMovement()->MaxWalkSpeed,
                Character->GetSprintSpeed());

            if (FPlatformTime::Seconds() - ExerciseStartTimeSeconds < 0.5)
            {
                return false;
            }

            InputSubsystem->InjectInputForAction(
                Character->GetMoveAction(),
                FInputActionValue(FVector2D::ZeroVector),
                {},
                {});
            InputSubsystem->InjectInputForAction(
                Character->GetLookAction(),
                FInputActionValue(FVector2D::ZeroVector),
                {},
                {});
            InputSubsystem->InjectInputForAction(
                Character->GetSprintAction(),
                FInputActionValue(false),
                {},
                {});
            ReleaseTimeSeconds = FPlatformTime::Seconds();
            bReleasedInput = true;
            return false;
        }

        if (FPlatformTime::Seconds() - ReleaseTimeSeconds < 0.15)
        {
            return false;
        }

        Test->TestTrue(TEXT("Sprint input reaches 660 cm/s in PIE"), bObservedSprintSpeed);
        Test->TestEqual(
            TEXT("Releasing Sprint restores 520 cm/s in PIE"),
            Character->GetCharacterMovement()->MaxWalkSpeed,
            520.0f);
        Test->TestTrue(
            TEXT("Move input changes the Character capsule location in PIE"),
            FVector::DistSquared2D(InitialLocation, Character->GetActorLocation()) > FMath::Square(10.0f));
        Test->TestTrue(
            TEXT("Look input changes local ControlRotation yaw in PIE"),
            !FMath::IsNearlyEqual(InitialControlYaw, PlayerController->GetControlRotation().Yaw));

        return true;
    }

private:
    FAutomationTestBase* Test;
    double StartTimeSeconds;
    double ExerciseStartTimeSeconds = 0.0;
    double ReleaseTimeSeconds = 0.0;
    FVector InitialLocation = FVector::ZeroVector;
    float InitialControlYaw = 0.0f;
    bool bInitialized = false;
    bool bReleasedInput = false;
    bool bObservedSprintSpeed = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathInputBindingsTest,
    "VELRATH.Input.Bindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathInputBindingsTest::RunTest(const FString& Parameters)
{
    const AVelrathCharacter* CharacterDefaults = GetDefault<AVelrathCharacter>();
    TestNotNull(TEXT("The VELRATH Character default object exists"), CharacterDefaults);
    if (CharacterDefaults == nullptr)
    {
        return false;
    }

    const UInputMappingContext* MappingContext = CharacterDefaults->GetDefaultMappingContext();
    const UInputAction* MoveAction = CharacterDefaults->GetMoveAction();
    const UInputAction* LookAction = CharacterDefaults->GetLookAction();
    const UInputAction* SprintAction = CharacterDefaults->GetSprintAction();
    TestNotNull(TEXT("Default Input Mapping Context exists"), MappingContext);
    TestNotNull(TEXT("Move Input Action exists"), MoveAction);
    TestNotNull(TEXT("Look Input Action exists"), LookAction);
    TestNotNull(TEXT("Sprint Input Action exists"), SprintAction);
    if (MappingContext == nullptr || MoveAction == nullptr || LookAction == nullptr || SprintAction == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("Move is an Axis2D action"), MoveAction->ValueType, EInputActionValueType::Axis2D);
    TestEqual(TEXT("Look is an Axis2D action"), LookAction->ValueType, EInputActionValueType::Axis2D);
    TestEqual(TEXT("Sprint is a Boolean action"), SprintAction->ValueType, EInputActionValueType::Boolean);
    TestEqual(TEXT("The native context contains exactly the requested six key mappings"), MappingContext->GetMappings().Num(), 6);

    const FEnhancedActionKeyMapping* ForwardMapping = FindMapping(*MappingContext, MoveAction, EKeys::W);
    const FEnhancedActionKeyMapping* BackwardMapping = FindMapping(*MappingContext, MoveAction, EKeys::S);
    const FEnhancedActionKeyMapping* LeftMapping = FindMapping(*MappingContext, MoveAction, EKeys::A);
    const FEnhancedActionKeyMapping* RightMapping = FindMapping(*MappingContext, MoveAction, EKeys::D);
    TestNotNull(TEXT("W maps to Move"), ForwardMapping);
    TestNotNull(TEXT("S maps to Move"), BackwardMapping);
    TestNotNull(TEXT("A maps to Move"), LeftMapping);
    TestNotNull(TEXT("D maps to Move"), RightMapping);
    TestNotNull(TEXT("Mouse X/Y maps to Look"), FindMapping(*MappingContext, LookAction, EKeys::Mouse2D));
    TestNotNull(TEXT("Left Shift maps to Sprint"), FindMapping(*MappingContext, SprintAction, EKeys::LeftShift));

    TestTrue(
        TEXT("W is swizzled onto the Move Y axis"),
        ForwardMapping != nullptr && ForwardMapping->Modifiers.Num() == 1 &&
            ForwardMapping->Modifiers[0]->IsA<UInputModifierSwizzleAxis>());
    TestTrue(
        TEXT("S is negated and swizzled onto the Move Y axis"),
        BackwardMapping != nullptr && BackwardMapping->Modifiers.Num() == 2 &&
            BackwardMapping->Modifiers[0]->IsA<UInputModifierNegate>() &&
            BackwardMapping->Modifiers[1]->IsA<UInputModifierSwizzleAxis>());
    TestTrue(
        TEXT("A is negated on the Move X axis"),
        LeftMapping != nullptr && LeftMapping->Modifiers.Num() == 1 &&
            LeftMapping->Modifiers[0]->IsA<UInputModifierNegate>());
    TestTrue(TEXT("D stays positive on the Move X axis"), RightMapping != nullptr && RightMapping->Modifiers.IsEmpty());

    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull(TEXT("An Unreal test world is available"), World);
    AVelrathCharacter* Character = World != nullptr ? World->SpawnActor<AVelrathCharacter>() : nullptr;
    TestNotNull(TEXT("A VELRATH Character can be spawned"), Character);
    if (Character == nullptr)
    {
        return false;
    }

    UEnhancedInputComponent* InputComponent = NewObject<UEnhancedInputComponent>(Character);
    Character->SetupPlayerInputComponent(InputComponent);
    TestEqual(TEXT("Move, Look, and Sprint expose five runtime event bindings"), InputComponent->GetActionEventBindings().Num(), 5);

    const FEnhancedInputActionEventBinding* MoveBinding =
        FindBinding(*InputComponent, Character->GetMoveAction(), ETriggerEvent::Triggered);
    const FEnhancedInputActionEventBinding* LookBinding =
        FindBinding(*InputComponent, Character->GetLookAction(), ETriggerEvent::Triggered);
    const FEnhancedInputActionEventBinding* SprintStartedBinding =
        FindBinding(*InputComponent, Character->GetSprintAction(), ETriggerEvent::Started);
    const FEnhancedInputActionEventBinding* SprintCompletedBinding =
        FindBinding(*InputComponent, Character->GetSprintAction(), ETriggerEvent::Completed);
    const FEnhancedInputActionEventBinding* SprintCanceledBinding =
        FindBinding(*InputComponent, Character->GetSprintAction(), ETriggerEvent::Canceled);
    TestNotNull(TEXT("Move binds Triggered"), MoveBinding);
    TestNotNull(TEXT("Look binds Triggered"), LookBinding);
    TestNotNull(TEXT("Sprint binds Started"), SprintStartedBinding);
    TestNotNull(TEXT("Sprint binds Completed"), SprintCompletedBinding);
    TestNotNull(TEXT("Sprint binds Canceled"), SprintCanceledBinding);

    const FInputActionInstance SprintInstance(Character->GetSprintAction());
    TestEqual(TEXT("Character starts at walk speed"), Character->GetCharacterMovement()->MaxWalkSpeed, 520.0f);
    if (SprintStartedBinding != nullptr)
    {
        SprintStartedBinding->Execute(SprintInstance);
        TestEqual(TEXT("Sprint Started switches to 660 cm/s"), Character->GetCharacterMovement()->MaxWalkSpeed, 660.0f);
    }
    if (SprintCompletedBinding != nullptr)
    {
        SprintCompletedBinding->Execute(SprintInstance);
        TestEqual(TEXT("Sprint Completed restores 520 cm/s"), Character->GetCharacterMovement()->MaxWalkSpeed, 520.0f);
    }
    if (SprintStartedBinding != nullptr && SprintCanceledBinding != nullptr)
    {
        SprintStartedBinding->Execute(SprintInstance);
        SprintCanceledBinding->Execute(SprintInstance);
        TestEqual(TEXT("Sprint Canceled restores 520 cm/s"), Character->GetCharacterMovement()->MaxWalkSpeed, 520.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathInputPieTest,
    "VELRATH.Input.OnePlayerPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathInputPieTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/L_Dev")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FValidateVelrathInputPieCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

    return true;
}

#endif
