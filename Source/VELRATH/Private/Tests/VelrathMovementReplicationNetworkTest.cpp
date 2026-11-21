#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#include "EnhancedInputSubsystems.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Game/VelrathGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputActionValue.h"
#include "Player/VelrathCharacter.h"
#include "Player/VelrathCharacterMovementComponent.h"

#if ENABLE_PIE_NETWORK_TEST

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogVelrathMovementNetworkTest, Log, All);

constexpr int32 OwningClientIndex = 0;
constexpr int32 RemoteClientIndex = 1;
constexpr float WalkSpeed = 520.0f;
constexpr float SprintSpeed = 660.0f;
constexpr float SpeedTolerance = 8.0f;
constexpr float PositionTolerance = 10.0f;
constexpr float StopSpeedTolerance = 1.0f;
constexpr float InputPhaseSeconds = 1.0f;
constexpr float AdversarialPhaseSeconds = 0.25f;

struct FVelrathMovementNetworkState : public FBasePIENetworkComponentState
{
    AVelrathCharacter* TargetCharacter = nullptr;
    AVelrathCharacter* PreviousTargetCharacter = nullptr;
    double PhaseStartSeconds = 0.0;
    float MaxObservedPlanarSpeed = 0.0f;
};

AVelrathCharacter* FindCharacterForPlayerId(UWorld& World, const int32 PlayerId)
{
    for (TActorIterator<AVelrathCharacter> It(&World); It; ++It)
    {
        AVelrathCharacter* Character = *It;
        const APlayerState* PlayerState = Character->GetPlayerState();
        if (IsValid(Character) && PlayerState != nullptr && PlayerState->GetPlayerId() == PlayerId)
        {
            return Character;
        }
    }

    return nullptr;
}

UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(const AVelrathCharacter& Character)
{
    const APlayerController* PlayerController = Cast<APlayerController>(Character.GetController());
    const ULocalPlayer* LocalPlayer = PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
    return LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

void InjectMovementInput(AVelrathCharacter& Character, const bool bMoveForward, const bool bSprint)
{
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetInputSubsystem(Character);
    check(InputSubsystem != nullptr);

    InputSubsystem->InjectInputForAction(
        Character.GetMoveAction(),
        FInputActionValue(bMoveForward ? FVector2D(0.0f, 1.0f) : FVector2D::ZeroVector),
        {},
        {});
    InputSubsystem->InjectInputForAction(
        Character.GetSprintAction(),
        FInputActionValue(bSprint),
        {},
        {});
}

float PlanarDistance(const FVector& A, const FVector& B)
{
    return FVector::Dist2D(A, B);
}

void SpawnMovementTestFloor(UWorld& World, const FVector& CharacterLocation)
{
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Floor = World.SpawnActor<AStaticMeshActor>(
        CharacterLocation - FVector(0.0f, 0.0f, 140.0f),
        FRotator::ZeroRotator,
        SpawnParameters);
    check(Floor != nullptr);
    Floor->SetReplicates(false);

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    check(CubeMesh != nullptr);
    UStaticMeshComponent* FloorMesh = Floor->GetStaticMeshComponent();
    FloorMesh->SetMobility(EComponentMobility::Movable);
    FloorMesh->SetStaticMesh(CubeMesh);
    FloorMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Floor->SetActorScale3D(FVector(50.0f, 50.0f, 1.0f));
    FloorMesh->SetMobility(EComponentMobility::Static);
}
}

NETWORK_TEST_CLASS(FVelrathMovementReplicationNetworkTest, "VELRATH.Network.Movement")
{
    FPIENetworkComponent<FVelrathMovementNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

    int32 TargetPlayerId = INDEX_NONE;
    FVector InitialServerLocation = FVector::ZeroVector;
    FVector ServerStoppedLocation = FVector::ZeroVector;
    FVector OwnerStoppedLocation = FVector::ZeroVector;
    FVector RemoteStoppedLocation = FVector::ZeroVector;
    FVector ServerCorrectedLocation = FVector::ZeroVector;
    FVector OwnerCorrectedLocation = FVector::ZeroVector;
    FVector RemoteCorrectedLocation = FVector::ZeroVector;
    FVector ServerBeforeAdversarialInput = FVector::ZeroVector;
    float ServerWalkSpeed = 0.0f;
    float OwnerWalkSpeed = 0.0f;
    float RemoteWalkSpeed = 0.0f;
    float ServerSprintSpeed = 0.0f;
    float OwnerSprintSpeed = 0.0f;
    float RemoteSprintSpeed = 0.0f;
    float MaxStoppedError = 0.0f;
    float MaxCorrectionError = 0.0f;

    BEFORE_EACH()
    {
        FNetworkComponentBuilder<FVelrathMovementNetworkState>()
            .WithClients(2)
            .AsListenServer()
            .WithGameInstanceClass(UGameInstance::StaticClass())
            .WithGameMode(AVelrathGameMode::StaticClass())
            .Build(Network);
    }

    TEST_METHOD(OwnerInput_ReplicatesCorrectsAndSurvivesPawnLifecycle)
    {
        Network
            .UntilServer(TEXT("Wait for two real client connections and target pawn"), [this](FVelrathMovementNetworkState& State)
            {
                if (State.ClientConnections.Num() != 2 || State.ClientConnections[OwningClientIndex] == nullptr)
                {
                    return false;
                }

                APlayerController* TargetController = State.ClientConnections[OwningClientIndex]->PlayerController;
                AVelrathCharacter* Character = TargetController != nullptr
                    ? Cast<AVelrathCharacter>(TargetController->GetPawn())
                    : nullptr;
                if (Character == nullptr || Character->GetPlayerState() == nullptr)
                {
                    return false;
                }

                State.TargetCharacter = Character;
                TargetPlayerId = Character->GetPlayerState()->GetPlayerId();
                return TargetPlayerId != INDEX_NONE;
            })
            .ThenServer(TEXT("Validate listen server and isolate the observer pawn"), [this](FVelrathMovementNetworkState& State)
            {
                ASSERT_THAT(IsTrue(State.World->GetNetMode() == NM_ListenServer, TEXT("Movement test must use a listen server.")));
                const UNetDriver* NetDriver = State.World->GetNetDriver();
                ASSERT_THAT(IsTrue(
                    NetDriver != nullptr && NetDriver->GetClass()->GetName().Contains(TEXT("IpNetDriver")),
                    TEXT("Movement test must use the real IpNetDriver.")));
                ASSERT_THAT(AreEqual(2, State.ClientConnections.Num(), TEXT("Movement test requires two UNetConnections.")));

                APlayerController* ObserverController = State.ClientConnections[RemoteClientIndex]->PlayerController;
                AVelrathCharacter* ObserverCharacter = ObserverController != nullptr
                    ? Cast<AVelrathCharacter>(ObserverController->GetPawn())
                    : nullptr;
                if (ObserverCharacter != nullptr)
                {
                    ObserverCharacter->SetActorLocation(
                        State.TargetCharacter->GetActorLocation() + FVector(1200.0f, 0.0f, 0.0f),
                        false,
                        nullptr,
                        ETeleportType::TeleportPhysics);
                    ObserverCharacter->ForceNetUpdate();
                }

                ASSERT_THAT(IsTrue(State.TargetCharacter->HasAuthority(), TEXT("Server target pawn must be authoritative.")));
                ASSERT_THAT(IsNotNull(State.TargetCharacter->GetVelrathCharacterMovement(), TEXT("Target must use the predicted VELRATH movement component.")));
            })
            .UntilClients(TEXT("Resolve target pawn on owner and remote"), [this](FVelrathMovementNetworkState& State)
            {
                State.TargetCharacter = FindCharacterForPlayerId(*State.World, TargetPlayerId);
                return State.TargetCharacter != nullptr;
            })
            .ThenServer(TEXT("Create server movement test floor"), [](FVelrathMovementNetworkState& State)
            {
                SpawnMovementTestFloor(*State.World, State.TargetCharacter->GetActorLocation());
            })
            .ThenClients(TEXT("Create client movement test floors"), [](FVelrathMovementNetworkState& State)
            {
                SpawnMovementTestFloor(*State.World, State.TargetCharacter->GetActorLocation());
            })
            .UntilServer(TEXT("Wait for authoritative grounded movement"), [this](FVelrathMovementNetworkState& State)
            {
                if (!State.TargetCharacter->GetCharacterMovement()->IsMovingOnGround())
                {
                    return false;
                }

                InitialServerLocation = State.TargetCharacter->GetActorLocation();
                return true;
            })
            .UntilClient(TEXT("Wait for grounded owning client movement"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                return State.TargetCharacter->GetCharacterMovement()->IsMovingOnGround();
            })
            .ThenClient(TEXT("Set camera yaw and prepare walk input"), OwningClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                ASSERT_THAT(IsTrue(LocalController != nullptr && LocalController->GetPawn() == State.TargetCharacter, TEXT("Owning client must possess the target pawn.")));
                ASSERT_THAT(IsNotNull(GetInputSubsystem(*State.TargetCharacter), TEXT("Owning client must have Enhanced Input.")));
                LocalController->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
                State.PhaseStartSeconds = State.World->GetTimeSeconds();
                State.MaxObservedPlanarSpeed = 0.0f;
            })
            .UntilClient(TEXT("Move owner through Enhanced Input at walk speed"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                InjectMovementInput(*State.TargetCharacter, true, false);
                State.MaxObservedPlanarSpeed = FMath::Max(
                    State.MaxObservedPlanarSpeed,
                    State.TargetCharacter->GetVelocity().Size2D());
                return State.World->GetTimeSeconds() - State.PhaseStartSeconds >= InputPhaseSeconds;
            })
            .ThenServer(TEXT("Measure authoritative walk movement"), [this](FVelrathMovementNetworkState& State)
            {
                ServerWalkSpeed = State.TargetCharacter->GetVelocity().Size2D();
                const FVector Delta = State.TargetCharacter->GetActorLocation() - InitialServerLocation;
                ASSERT_THAT(IsNear(WalkSpeed, ServerWalkSpeed, SpeedTolerance, TEXT("Server walk speed must be 520 cm/s.")));
                ASSERT_THAT(IsTrue(Delta.Y > 100.0f, TEXT("Yaw 90 forward input must move in positive world Y.")));
                ASSERT_THAT(IsTrue(FMath::Abs(Delta.X) < FMath::Abs(Delta.Y) * 0.25f, TEXT("Movement direction must follow camera yaw.")));
                ASSERT_THAT(IsFalse(State.TargetCharacter->GetVelrathCharacterMovement()->WantsToSprint(), TEXT("Server must remain in walk state.")));
            })
            .ThenClient(TEXT("Measure owner walk prediction"), OwningClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                OwnerWalkSpeed = State.MaxObservedPlanarSpeed;
                ASSERT_THAT(IsNear(WalkSpeed, OwnerWalkSpeed, SpeedTolerance, TEXT("Owner walk prediction must reach 520 cm/s.")));
                State.PhaseStartSeconds = State.World->GetTimeSeconds();
                State.MaxObservedPlanarSpeed = 0.0f;
            })
            .ThenClient(TEXT("Measure remote walk replication"), RemoteClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                RemoteWalkSpeed = State.TargetCharacter->GetVelocity().Size2D();
                ASSERT_THAT(IsTrue(State.TargetCharacter->GetLocalRole() == ROLE_SimulatedProxy, TEXT("Remote target must be a simulated proxy.")));
                ASSERT_THAT(IsTrue(PlanarDistance(InitialServerLocation, State.TargetCharacter->GetActorLocation()) > 100.0f, TEXT("Remote client must receive target movement.")));
            })
            .UntilClient(TEXT("Sprint owner through predicted Enhanced Input"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                InjectMovementInput(*State.TargetCharacter, true, true);
                State.MaxObservedPlanarSpeed = FMath::Max(
                    State.MaxObservedPlanarSpeed,
                    State.TargetCharacter->GetVelocity().Size2D());
                return State.World->GetTimeSeconds() - State.PhaseStartSeconds >= InputPhaseSeconds;
            })
            .ThenServer(TEXT("Measure authoritative predicted sprint"), [this](FVelrathMovementNetworkState& State)
            {
                ServerSprintSpeed = State.TargetCharacter->GetVelocity().Size2D();
                ASSERT_THAT(IsTrue(State.TargetCharacter->GetVelrathCharacterMovement()->WantsToSprint(), TEXT("Server must receive sprint in compressed movement flags.")));
                ASSERT_THAT(IsNear(SprintSpeed, ServerSprintSpeed, SpeedTolerance, TEXT("Server sprint speed must be 660 cm/s.")));
            })
            .ThenClient(TEXT("Measure owner sprint prediction and release input"), OwningClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                OwnerSprintSpeed = State.MaxObservedPlanarSpeed;
                ASSERT_THAT(IsNear(SprintSpeed, OwnerSprintSpeed, SpeedTolerance, TEXT("Owner sprint prediction must reach 660 cm/s.")));
                InjectMovementInput(*State.TargetCharacter, false, false);
            })
            .ThenClient(TEXT("Measure remote sprint replication"), RemoteClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                RemoteSprintSpeed = State.TargetCharacter->GetVelocity().Size2D();
            })
            .UntilServer(TEXT("Wait for server stop and sprint release"), [](FVelrathMovementNetworkState& State)
            {
                return !State.TargetCharacter->GetVelrathCharacterMovement()->WantsToSprint()
                    && State.TargetCharacter->GetVelocity().Size2D() <= StopSpeedTolerance;
            })
            .ThenServer(TEXT("Capture stopped authoritative location"), [this](FVelrathMovementNetworkState& State)
            {
                ASSERT_THAT(IsNear(WalkSpeed, State.TargetCharacter->GetCharacterMovement()->GetMaxSpeed(), UE_SMALL_NUMBER, TEXT("Sprint release must restore 520 cm/s.")));
                ServerStoppedLocation = State.TargetCharacter->GetActorLocation();
            })
            .UntilClients(TEXT("Wait for owner and remote stop convergence"), [this](FVelrathMovementNetworkState& State)
            {
                return State.TargetCharacter->GetVelocity().Size2D() <= StopSpeedTolerance
                    && PlanarDistance(ServerStoppedLocation, State.TargetCharacter->GetActorLocation()) <= PositionTolerance;
            })
            .ThenClient(TEXT("Capture stopped owner location"), OwningClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                OwnerStoppedLocation = State.TargetCharacter->GetActorLocation();
            })
            .ThenClient(TEXT("Capture stopped remote location"), RemoteClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                RemoteStoppedLocation = State.TargetCharacter->GetActorLocation();
                MaxStoppedError = FMath::Max3(
                    PlanarDistance(ServerStoppedLocation, OwnerStoppedLocation),
                    PlanarDistance(ServerStoppedLocation, RemoteStoppedLocation),
                    PlanarDistance(OwnerStoppedLocation, RemoteStoppedLocation));
                ASSERT_THAT(IsTrue(MaxStoppedError <= PositionTolerance, TEXT("Server, owner, and remote must converge after stopping.")));
            })
            .ThenServer(TEXT("Remember authoritative position before adversarial client move"), [this](FVelrathMovementNetworkState& State)
            {
                ServerBeforeAdversarialInput = State.TargetCharacter->GetActorLocation();
            })
            .ThenClient(TEXT("Attempt client-only teleport and invalid local speed"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                State.TargetCharacter->GetCharacterMovement()->MaxWalkSpeed = 5000.0f;
                State.TargetCharacter->SetActorLocation(
                    State.TargetCharacter->GetActorLocation() + FVector(4000.0f, 4000.0f, 0.0f),
                    false,
                    nullptr,
                    ETeleportType::TeleportPhysics);
                State.PhaseStartSeconds = State.World->GetTimeSeconds();
            })
            .UntilClient(TEXT("Send adversarial movement through normal ServerMove"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                InjectMovementInput(*State.TargetCharacter, true, false);
                return State.World->GetTimeSeconds() - State.PhaseStartSeconds >= AdversarialPhaseSeconds;
            })
            .ThenClient(TEXT("Restore approved local speed and stop"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                State.TargetCharacter->GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
                InjectMovementInput(*State.TargetCharacter, false, false);
            })
            .UntilServer(TEXT("Verify authority rejects teleport and invalid speed"), [this](FVelrathMovementNetworkState& State)
            {
                const float ServerSpeed = State.TargetCharacter->GetVelocity().Size2D();
                const float ServerDisplacement = PlanarDistance(ServerBeforeAdversarialInput, State.TargetCharacter->GetActorLocation());
                return ServerSpeed <= StopSpeedTolerance && ServerDisplacement < 300.0f;
            })
            .ThenServer(TEXT("Capture authoritative correction result"), [this](FVelrathMovementNetworkState& State)
            {
                const float ServerDisplacement = PlanarDistance(ServerBeforeAdversarialInput, State.TargetCharacter->GetActorLocation());
                ASSERT_THAT(IsTrue(ServerDisplacement < 300.0f, TEXT("Server must reject the 5657 cm client-only teleport.")));
                ASSERT_THAT(IsNear(WalkSpeed, State.TargetCharacter->GetCharacterMovement()->MaxWalkSpeed, UE_SMALL_NUMBER, TEXT("Client speed tampering must not change server MaxWalkSpeed.")));
                ServerCorrectedLocation = State.TargetCharacter->GetActorLocation();
            })
            .UntilClients(TEXT("Wait for correction convergence"), [this](FVelrathMovementNetworkState& State)
            {
                return State.TargetCharacter->GetVelocity().Size2D() <= StopSpeedTolerance
                    && PlanarDistance(ServerCorrectedLocation, State.TargetCharacter->GetActorLocation()) <= PositionTolerance;
            })
            .ThenClient(TEXT("Capture corrected owner location"), OwningClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                OwnerCorrectedLocation = State.TargetCharacter->GetActorLocation();
            })
            .ThenClient(TEXT("Capture corrected remote location"), RemoteClientIndex, [this](FVelrathMovementNetworkState& State)
            {
                RemoteCorrectedLocation = State.TargetCharacter->GetActorLocation();
                MaxCorrectionError = FMath::Max3(
                    PlanarDistance(ServerCorrectedLocation, OwnerCorrectedLocation),
                    PlanarDistance(ServerCorrectedLocation, RemoteCorrectedLocation),
                    PlanarDistance(OwnerCorrectedLocation, RemoteCorrectedLocation));
                ASSERT_THAT(IsTrue(MaxCorrectionError <= PositionTolerance, TEXT("All views must converge after authoritative correction.")));
            })
            .ThenServer(TEXT("Repossess the same pawn"), [this](FVelrathMovementNetworkState& State)
            {
                APlayerController* Controller = Cast<APlayerController>(State.TargetCharacter->GetController());
                ASSERT_THAT(IsNotNull(Controller, TEXT("Target pawn must have a server controller.")));
                Controller->UnPossess();
                Controller->Possess(State.TargetCharacter);
                ASSERT_THAT(IsTrue(Controller->GetPawn() == State.TargetCharacter, TEXT("Server must repossess the same pawn.")));
                ASSERT_THAT(IsFalse(State.TargetCharacter->GetVelrathCharacterMovement()->WantsToSprint(), TEXT("Repossession must clear sprint intent.")));
            })
            .UntilClient(TEXT("Wait for owner after repossession"), OwningClientIndex, [](FVelrathMovementNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                return LocalController != nullptr
                    && LocalController->GetPawn() == State.TargetCharacter
                    && !State.TargetCharacter->GetVelrathCharacterMovement()->WantsToSprint()
                    && FMath::IsNearlyEqual(State.TargetCharacter->GetCharacterMovement()->GetMaxSpeed(), WalkSpeed);
            })
            .ThenClients(TEXT("Remember old pawn before respawn"), [](FVelrathMovementNetworkState& State)
            {
                State.PreviousTargetCharacter = State.TargetCharacter;
            })
            .ThenServer(TEXT("Destroy and respawn authoritative pawn"), [this](FVelrathMovementNetworkState& State)
            {
                State.PreviousTargetCharacter = State.TargetCharacter;
                APlayerController* Controller = Cast<APlayerController>(State.TargetCharacter->GetController());
                AGameModeBase* GameMode = State.World->GetAuthGameMode();
                ASSERT_THAT(IsNotNull(Controller, TEXT("Respawn requires target server controller.")));
                ASSERT_THAT(IsNotNull(GameMode, TEXT("Respawn requires authoritative GameMode.")));

                Controller->UnPossess();
                State.PreviousTargetCharacter->Destroy();
                GameMode->RestartPlayer(Controller);
                State.TargetCharacter = Cast<AVelrathCharacter>(Controller->GetPawn());

                ASSERT_THAT(IsNotNull(State.TargetCharacter, TEXT("Respawn must create a VELRATH Character.")));
                ASSERT_THAT(IsTrue(State.TargetCharacter != State.PreviousTargetCharacter, TEXT("Respawn must replace the pawn.")));
                ASSERT_THAT(IsFalse(State.TargetCharacter->GetVelrathCharacterMovement()->WantsToSprint(), TEXT("Respawn must start without sprint intent.")));
                ASSERT_THAT(IsNear(WalkSpeed, State.TargetCharacter->GetCharacterMovement()->GetMaxSpeed(), UE_SMALL_NUMBER, TEXT("Respawn must start at 520 cm/s.")));
            })
            .UntilClients(TEXT("Wait for clean replacement pawn"), [this](FVelrathMovementNetworkState& State)
            {
                AVelrathCharacter* Replacement = FindCharacterForPlayerId(*State.World, TargetPlayerId);
                if (Replacement == nullptr || Replacement == State.PreviousTargetCharacter)
                {
                    return false;
                }

                State.TargetCharacter = Replacement;
                return !Replacement->GetVelrathCharacterMovement()->WantsToSprint()
                    && FMath::IsNearlyEqual(Replacement->GetCharacterMovement()->MaxWalkSpeed, WalkSpeed);
            })
            .Then(TEXT("Record movement replication evidence"), [this]()
            {
                TestRunner->AddInfo(FString::Printf(
                    TEXT("FND-006 PASS: walk server/owner/remote=%.2f/%.2f/%.2f cm/s; sprint server/owner/remote=%.2f/%.2f/%.2f cm/s; stopped server=%s owner=%s remote=%s maxError=%.2f cm; corrected server=%s owner=%s remote=%s maxError=%.2f cm."),
                    ServerWalkSpeed,
                    OwnerWalkSpeed,
                    RemoteWalkSpeed,
                    ServerSprintSpeed,
                    OwnerSprintSpeed,
                    RemoteSprintSpeed,
                    *ServerStoppedLocation.ToCompactString(),
                    *OwnerStoppedLocation.ToCompactString(),
                    *RemoteStoppedLocation.ToCompactString(),
                    MaxStoppedError,
                    *ServerCorrectedLocation.ToCompactString(),
                    *OwnerCorrectedLocation.ToCompactString(),
                    *RemoteCorrectedLocation.ToCompactString(),
                    MaxCorrectionError));
                UE_LOG(LogVelrathMovementNetworkTest, Display,
                    TEXT("FND-006 metrics: walk server/owner/remote=%.2f/%.2f/%.2f sprint=%.2f/%.2f/%.2f stoppedError=%.2f correctionError=%.2f"),
                    ServerWalkSpeed,
                    OwnerWalkSpeed,
                    RemoteWalkSpeed,
                    ServerSprintSpeed,
                    OwnerSprintSpeed,
                    RemoteSprintSpeed,
                    MaxStoppedError,
                    MaxCorrectionError);
            });
    }
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
