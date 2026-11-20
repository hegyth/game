#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Player/VelrathCharacter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace
{
constexpr double DevEnvironmentPieTimeoutSeconds = 15.0;

class FCaptureVelrathDevEnvironmentPieCommand final : public IAutomationLatentCommand
{
public:
    virtual bool Update() override
    {
        if (!FApp::CanEverRender())
        {
            return true;
        }

        if (!bScreenshotRequested)
        {
            const FString ScreenshotPath = FPaths::Combine(
                FPaths::ProjectSavedDir(),
                TEXT("Automation/DEV001_PIE.png"));
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
            bScreenshotRequested = true;
            StartTimeSeconds = FPlatformTime::Seconds();
            return false;
        }

        return !FScreenshotRequest::IsScreenshotRequested() ||
            FPlatformTime::Seconds() - StartTimeSeconds >= DevEnvironmentPieTimeoutSeconds;
    }

private:
    bool bScreenshotRequested = false;
    double StartTimeSeconds = 0.0;
};

class FValidateVelrathDevEnvironmentPieCommand final : public IAutomationLatentCommand
{
public:
    explicit FValidateVelrathDevEnvironmentPieCommand(FAutomationTestBase* InTest)
        : Test(InTest)
        , StartTimeSeconds(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        if (GEngine == nullptr)
        {
            Test->AddError(TEXT("GEngine is unavailable while waiting for PIE."));
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

        if (PieWorld == nullptr || PieWorld->GetFirstPlayerController() == nullptr ||
            PieWorld->GetFirstPlayerController()->GetPawn() == nullptr)
        {
            if (FPlatformTime::Seconds() - StartTimeSeconds >= DevEnvironmentPieTimeoutSeconds)
            {
                Test->AddError(TEXT("Timed out waiting for a possessed Pawn in one-player PIE."));
                return true;
            }

            return false;
        }

        APlayerController* PlayerController = PieWorld->GetFirstPlayerController();
        APawn* Pawn = PlayerController->GetPawn();
        AVelrathCharacter* Character = Cast<AVelrathCharacter>(Pawn);

        Test->TestNotNull(TEXT("PIE creates a VELRATH Character Pawn"), Character);
        if (Character == nullptr)
        {
            return true;
        }

        Test->TestEqual(
            TEXT("PIE spawns BP_VelrathCharacter"),
            Character->GetClass()->GetPathName(),
            FString(TEXT("/Game/Characters/BP_VelrathCharacter.BP_VelrathCharacter_C")));
        Test->TestTrue(TEXT("PlayerController possesses the spawned Character"), PlayerController->GetPawn() == Character);

        USkeletalMeshComponent* Mesh = Character->GetMesh();
        Test->TestNotNull(TEXT("Character has a SkeletalMeshComponent"), Mesh);
        Test->TestNotNull(TEXT("Character has an assigned Skeletal Mesh"), Mesh != nullptr ? Mesh->GetSkeletalMeshAsset() : nullptr);
        if (Mesh != nullptr && Mesh->GetSkeletalMeshAsset() != nullptr)
        {
            Test->TestEqual(
                TEXT("Character uses the migrated Manny mesh"),
                Mesh->GetSkeletalMeshAsset()->GetPathName(),
                FString(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
        }

        UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
        USpringArmComponent* SpringArm = Character->FindComponentByClass<USpringArmComponent>();
        Test->TestNotNull(TEXT("Character has a CameraComponent"), Camera);
        Test->TestNotNull(TEXT("Character has a SpringArmComponent"), SpringArm);
        Test->TestTrue(TEXT("Follow camera is active"), Camera != nullptr && Camera->IsActive());

        APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
        Test->TestNotNull(TEXT("PlayerController has a PlayerCameraManager"), CameraManager);
        Test->TestTrue(
            TEXT("Player camera uses the possessed Character as its view target"),
            CameraManager != nullptr && CameraManager->GetViewTarget() == Character);

        AGameModeBase* GameMode = PieWorld->GetAuthGameMode();
        Test->TestNotNull(TEXT("PIE creates the authoritative GameMode"), GameMode);
        if (GameMode != nullptr)
        {
            const UClass* DefaultPawnClass = GameMode->GetDefaultPawnClassForController(PlayerController);
            Test->TestNotNull(TEXT("GameMode resolves a DefaultPawnClass"), DefaultPawnClass);
            if (DefaultPawnClass != nullptr)
            {
                Test->TestEqual(
                    TEXT("GameMode resolves BP_VelrathCharacter as DefaultPawnClass"),
                    DefaultPawnClass->GetPathName(),
                    FString(TEXT("/Game/Characters/BP_VelrathCharacter.BP_VelrathCharacter_C")));
            }
        }

        return true;
    }

private:
    FAutomationTestBase* Test;
    double StartTimeSeconds;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathDevEnvironmentPieTest,
    "VELRATH.DevEnvironment.OnePlayerPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathDevEnvironmentPieTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/L_Dev")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FValidateVelrathDevEnvironmentPieCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FCaptureVelrathDevEnvironmentPieCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

    return true;
}

#endif
