#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#include "Combat/VelrathCombatClockSubsystem.h"
#include "Game/VelrathGameMode.h"
#include "Game/VelrathGameState.h"

#if ENABLE_PIE_NETWORK_TEST

namespace
{
struct FVelrathCombatClockNetworkState : public FBasePIENetworkComponentState
{
    UVelrathCombatClockSubsystem* ClockSubsystem = nullptr;
    AVelrathGameState* GameState = nullptr;
};
}

NETWORK_TEST_CLASS(FVelrathCombatClockNetworkTest, "VELRATH.Combat.Clock.Network.Authority")
{
    FPIENetworkComponent<FVelrathCombatClockNetworkState> Network{
        TestRunner,
        TestCommandBuilder,
        bInitializing};
    FGuid ServerEpoch;
    uint32 InitialAnchorSequence = 0;
    UVelrathCombatClockSubsystem* ServerSubsystem = nullptr;
    UVelrathCombatClockSubsystem* ClientSubsystems[2] = {nullptr, nullptr};

    BEFORE_EACH()
    {
        FNetworkComponentBuilder<FVelrathCombatClockNetworkState>()
            .WithClients(2)
            .AsListenServer()
            .WithGameInstanceClass(UGameInstance::StaticClass())
            .WithGameMode(AVelrathGameMode::StaticClass())
            .Build(Network);
    }

    TEST_METHOD(ServerOwnsClock_ClientsOnlyConsumeSparseAnchors)
    {
        Network
            .UntilServer(TEXT("Wait for the authoritative world clock and first anchor"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    State.ClockSubsystem = State.World->GetSubsystem<UVelrathCombatClockSubsystem>();
                    State.GameState = State.World->GetGameState<AVelrathGameState>();
                    if (State.ClockSubsystem == nullptr || State.GameState == nullptr)
                    {
                        return false;
                    }

                    const FVelrathCombatClockAnchor& Anchor = State.GameState->GetCombatClockAnchor();
                    return State.ClockSubsystem->IsGameplayClockActive()
                        && State.ClockSubsystem->GetCurrentTick() > 0
                        && Anchor.IsValid()
                        && Anchor.ClockEpoch == State.ClockSubsystem->GetClockEpoch();
                })
            .ThenServer(TEXT("Validate server-world ownership and diagnostics"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    ServerSubsystem = State.ClockSubsystem;
                    ServerEpoch = State.ClockSubsystem->GetClockEpoch();
                    InitialAnchorSequence = State.GameState->GetCombatClockAnchor().AnchorSequence;

                    ASSERT_THAT(IsTrue(State.World->GetNetMode() == NM_ListenServer,
                        TEXT("The CQTest server world must be a listen server.")));
                    ASSERT_THAT(IsTrue(ServerEpoch.IsValid(),
                        TEXT("The authoritative gameplay world must create a nonzero epoch.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->GetOuter() == State.World,
                        TEXT("The authoritative clock subsystem must be owned by exactly its UWorld.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->GetLastWorldTickStepCount()
                            <= FVelrathCombatClock::MaxStepsPerWorldTick,
                        TEXT("Runtime diagnostics must never report more than eight steps.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->GetAccumulatedBacklogSeconds() >= 0.0,
                        TEXT("Runtime backlog diagnostics must remain non-negative.")));
                })
            .UntilClients(TEXT("Wait for sparse authoritative anchor replication"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    State.ClockSubsystem = State.World->GetSubsystem<UVelrathCombatClockSubsystem>();
                    State.GameState = State.World->GetGameState<AVelrathGameState>();
                    return State.ClockSubsystem != nullptr
                        && State.GameState != nullptr
                        && State.GameState->GetCombatClockAnchor().ClockEpoch == ServerEpoch;
                })
            .ThenClients(TEXT("Validate clients never run authoritative combat simulation"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    ASSERT_THAT(IsTrue(State.World->GetNetMode() == NM_Client,
                        TEXT("Both observer worlds must be clients.")));
                    ASSERT_THAT(IsFalse(State.ClockSubsystem->IsGameplayClockActive(),
                        TEXT("A client world must not activate an authoritative combat clock.")));
                    ASSERT_THAT(AreEqual(0ull, State.ClockSubsystem->GetCurrentTick(),
                        TEXT("A client subsystem CurrentTick must remain zero.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->GetOuter() == State.World,
                        TEXT("Each client subsystem must be owned by its own PIE UWorld.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem != ServerSubsystem,
                        TEXT("A client PIE world must not share the server subsystem instance.")));

                    const FVelrathCombatClockAnchor& Anchor = State.GameState->GetCombatClockAnchor();
                    const uint64 Estimate = State.GameState->GetEstimatedCombatTick();
                    ASSERT_THAT(IsTrue(Anchor.IsValid(),
                        TEXT("Clients must receive a valid sparse clock anchor.")));
                    ASSERT_THAT(IsTrue(Estimate >= Anchor.ServerTick,
                        TEXT("A client presentation estimate cannot precede its anchor.")));
                    ASSERT_THAT(IsTrue(Estimate <= Anchor.ServerTick + 30,
                        TEXT("A client presentation estimate must remain within the 30-tick cap.")));

                    if (State.ClientIndex >= 0 && State.ClientIndex < 2)
                    {
                        ClientSubsystems[State.ClientIndex] = State.ClockSubsystem;
                    }
                })
            .ThenServer(TEXT("Validate bounded hitch catch-up and backlog diagnostics"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    const uint64 TickBeforeHitch = State.ClockSubsystem->GetCurrentTick();
                    const uint32 SequenceBeforeHitch =
                        State.GameState->GetCombatClockAnchor().AnchorSequence;

                    State.ClockSubsystem->Tick(0.5f);
                    ASSERT_THAT(AreEqual(8u, State.ClockSubsystem->GetLastWorldTickStepCount(),
                        TEXT("A runtime 500 ms hitch must execute exactly eight immediate steps.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->DidLastWorldTickHitStepLimit(),
                        TEXT("Runtime diagnostics must record the eight-step limit.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->IsOverrun(),
                        TEXT("Runtime diagnostics must expose retained complete-step backlog.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->GetAccumulatedBacklogSeconds() > 0.0,
                        TEXT("The retained runtime backlog must be measurable in seconds.")));

                    State.ClockSubsystem->Tick(0.0f);
                    ASSERT_THAT(AreEqual(8u, State.ClockSubsystem->GetLastWorldTickStepCount(),
                        TEXT("A later world tick must drain at most eight backlog steps.")));
                    ASSERT_THAT(IsTrue(State.ClockSubsystem->HasSustainedBacklog(),
                        TEXT("Backlog spanning consecutive world ticks must be marked sustained.")));

                    int32 CatchUpCalls = 1;
                    while (State.ClockSubsystem->IsOverrun() && CatchUpCalls < 8)
                    {
                        State.ClockSubsystem->Tick(0.0f);
                        ++CatchUpCalls;
                    }

                    ASSERT_THAT(IsFalse(State.ClockSubsystem->IsOverrun(),
                        TEXT("Zero-delta world ticks must be able to drain retained backlog.")));
                    ASSERT_THAT(IsFalse(State.ClockSubsystem->HasSustainedBacklog(),
                        TEXT("Sustained-backlog diagnostics must clear after recovery.")));
                    ASSERT_THAT(AreEqual(TickBeforeHitch + 30ull, State.ClockSubsystem->GetCurrentTick(),
                        TEXT("Runtime hitch recovery must execute all 30 ticks without a jump.")));
                    ASSERT_THAT(IsTrue(
                        State.GameState->GetCombatClockAnchor().AnchorSequence >= SequenceBeforeHitch + 2,
                        TEXT("Overrun begin and end must each force a sparse anchor refresh.")));
                })
            .UntilServer(TEXT("Wait for the periodic 10 Hz anchor to refresh"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    return State.GameState->GetCombatClockAnchor().AnchorSequence
                        > InitialAnchorSequence;
                })
            .ThenServer(TEXT("Validate world-end teardown clears all runtime state"),
                [this](FVelrathCombatClockNetworkState& State)
                {
                    State.ClockSubsystem->OnWorldEndPlay(*State.World);
                    ASSERT_THAT(IsFalse(State.ClockSubsystem->IsGameplayClockActive(),
                        TEXT("World end play must stop the gameplay clock.")));
                    ASSERT_THAT(AreEqual(0ull, State.ClockSubsystem->GetCurrentTick(),
                        TEXT("World end play must clear the old CurrentTick.")));
                    ASSERT_THAT(IsFalse(State.ClockSubsystem->GetClockEpoch().IsValid(),
                        TEXT("World end play must invalidate the old epoch.")));
                    ASSERT_THAT(AreEqual(0u, State.ClockSubsystem->GetLastWorldTickStepCount(),
                        TEXT("World end play must clear step diagnostics.")));
                    ASSERT_THAT(IsFalse(State.ClockSubsystem->IsOverrun(),
                        TEXT("World end play must clear overrun state.")));
                })
            .Then(TEXT("Validate all PIE worlds own isolated subsystem instances"), [this]()
                {
                    ASSERT_THAT(IsNotNull(ClientSubsystems[0],
                        TEXT("The owning-client PIE world must expose its subsystem.")));
                    ASSERT_THAT(IsNotNull(ClientSubsystems[1],
                        TEXT("The remote-client PIE world must expose its subsystem.")));
                    ASSERT_THAT(IsTrue(ClientSubsystems[0] != ClientSubsystems[1],
                        TEXT("Owning and remote client worlds must not share a subsystem instance.")));
                    ASSERT_THAT(IsTrue(ClientSubsystems[0] != ServerSubsystem
                            && ClientSubsystems[1] != ServerSubsystem,
                        TEXT("No client world may share the authoritative server clock object.")));
                });
    }
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
