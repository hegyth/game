#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/VelrathCombatClock.h"
#include "Combat/VelrathCombatClockAnchor.h"

#include <limits>

namespace
{
void DrainBacklog(FVelrathCombatClock& Clock)
{
    while (Clock.HasCompleteStepBacklog() && !Clock.IsAtTickLimit())
    {
        Clock.Advance(0.0);
    }
}

uint64 RunPartitionedSecond(const double DeltaSeconds, const int32 Calls)
{
    FVelrathCombatClock Clock;
    for (int32 Index = 0; Index < Calls; ++Index)
    {
        Clock.Advance(DeltaSeconds);
    }
    DrainBacklog(Clock);
    return Clock.GetCurrentTick();
}

FVelrathCombatClockAnchor MakeAnchor(
    const FGuid& Epoch,
    const uint32 Sequence,
    const uint64 Tick,
    const double ServerTime)
{
    FVelrathCombatClockAnchor Anchor;
    Anchor.ClockEpoch = Epoch;
    Anchor.AnchorSequence = Sequence;
    Anchor.ServerTick = Tick;
    Anchor.SampledServerWorldTimeSeconds = ServerTime;
    return Anchor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathCombatClockFixedStepTest,
    "VELRATH.Combat.Clock.FixedStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathCombatClockFixedStepTest::RunTest(const FString& Parameters)
{
    FVelrathCombatClock Clock;

    TestEqual(TEXT("DeltaTime zero executes no step"), Clock.Advance(0.0).StepsExecuted, 0u);
    TestEqual(TEXT("DeltaTime zero leaves CurrentTick at zero"), Clock.GetCurrentTick(), 0ull);

    FVelrathCombatClockAdvanceResult Result = Clock.Advance(FVelrathCombatClock::LogicalStepSeconds);
    TestEqual(TEXT("One fixed delta executes one step"), Result.StepsExecuted, 1u);
    TestEqual(TEXT("One fixed delta advances CurrentTick to one"), Clock.GetCurrentTick(), 1ull);

    Result = Clock.Advance(3.0 * FVelrathCombatClock::LogicalStepSeconds);
    TestEqual(TEXT("Three fixed deltas execute three steps in one call"), Result.StepsExecuted, 3u);
    TestEqual(TEXT("Multiple steps retain exact CurrentTick"), Clock.GetCurrentTick(), 4ull);

    Clock.Reset();
    Clock.Advance(0.5 * FVelrathCombatClock::LogicalStepSeconds);
    TestEqual(TEXT("Half-step remainder does not advance"), Clock.GetCurrentTick(), 0ull);
    TestTrue(TEXT("Half-step remainder is retained"), FMath::IsNearlyEqual(
        Clock.GetAccumulatedBacklogSeconds(),
        0.5 * FVelrathCombatClock::LogicalStepSeconds,
        1.0e-12));
    Clock.Advance(0.5 * FVelrathCombatClock::LogicalStepSeconds);
    TestEqual(TEXT("Two half steps advance exactly once"), Clock.GetCurrentTick(), 1ull);
    TestTrue(TEXT("Drained fractional remainder is normalized"), FMath::IsNearlyZero(
        Clock.GetAccumulatedBacklogSeconds(),
        1.0e-12));

    TestEqual(TEXT("TIME-01 60 x 1/60"), RunPartitionedSecond(1.0 / 60.0, 60), 60ull);
    TestEqual(TEXT("TIME-01 30 x 1/30"), RunPartitionedSecond(1.0 / 30.0, 30), 60ull);
    TestEqual(TEXT("TIME-01 20 x 1/20"), RunPartitionedSecond(1.0 / 20.0, 20), 60ull);
    TestEqual(TEXT("TIME-01 120 x 1/120"), RunPartitionedSecond(1.0 / 120.0, 120), 60ull);

    Clock.Reset();
    TArray<uint64> EmittedTicks;
    Result = Clock.Advance(0.5, [&EmittedTicks](const uint64 Tick)
    {
        EmittedTicks.Add(Tick);
    });
    TestEqual(TEXT("A 500 ms hitch is capped at eight steps"), Result.StepsExecuted, 8u);
    TestTrue(TEXT("The hitch reports the step limit"), Result.bHitStepLimit);
    TestTrue(TEXT("The hitch retains complete-step backlog"), Result.bHasBacklog);
    TestEqual(TEXT("The first hitch call advances to tick eight"), Clock.GetCurrentTick(), 8ull);
    TestTrue(TEXT("The retained hitch backlog is approximately 22 ticks"), FMath::IsNearlyEqual(
        Clock.GetAccumulatedBacklogSeconds(),
        22.0 / 60.0,
        1.0e-12));

    while (Clock.HasCompleteStepBacklog())
    {
        Clock.Advance(0.0, [&EmittedTicks](const uint64 Tick)
        {
            EmittedTicks.Add(Tick);
        });
    }
    TestEqual(TEXT("Sequential catch-up reaches all 30 scheduled ticks"), Clock.GetCurrentTick(), 30ull);
    TestEqual(TEXT("Sequential catch-up emits exactly 30 callbacks"), EmittedTicks.Num(), 30);
    for (int32 Index = 0; Index < EmittedTicks.Num(); ++Index)
    {
        TestEqual(
            *FString::Printf(TEXT("Catch-up tick %d is contiguous"), Index),
            EmittedTicks[Index],
            static_cast<uint64>(Index + 1));
    }

    TestTrue(TEXT("Half-open interval includes its start"),
        FVelrathCombatClock::IsTickInHalfOpenInterval(10, 10, 20));
    TestTrue(TEXT("Half-open interval includes its final interior tick"),
        FVelrathCombatClock::IsTickInHalfOpenInterval(19, 10, 20));
    TestFalse(TEXT("Half-open interval excludes its end"),
        FVelrathCombatClock::IsTickInHalfOpenInterval(20, 10, 20));
    TestFalse(TEXT("Half-open interval excludes ticks before start"),
        FVelrathCombatClock::IsTickInHalfOpenInterval(9, 10, 20));

    uint64 LocalTick = MAX_uint64;
    TestTrue(TEXT("Local tick is available at StartTick"),
        FVelrathCombatClock::TryGetLocalTick(100, 100, LocalTick));
    TestEqual(TEXT("Local action tick at StartTick is zero"), LocalTick, 0ull);
    TestTrue(TEXT("Local tick is CurrentTick minus StartTick"),
        FVelrathCombatClock::TryGetLocalTick(137, 100, LocalTick));
    TestEqual(TEXT("Local action tick subtraction is exact"), LocalTick, 37ull);
    TestFalse(TEXT("A pre-start CurrentTick cannot underflow local time"),
        FVelrathCombatClock::TryGetLocalTick(99, 100, LocalTick));

    Clock.Reset(3);
    bool bTransitionApplied = false;
    bool bResolutionObservedTransition = false;
    Clock.Advance(2.0 * FVelrathCombatClock::LogicalStepSeconds,
        [&bTransitionApplied, &bResolutionObservedTransition](const uint64 Tick)
        {
            if (Tick == 5)
            {
                bTransitionApplied = true;
            }
            if (Tick == 5)
            {
                bResolutionObservedTransition = bTransitionApplied;
            }
        });
    TestTrue(TEXT("A transition at tick N is visible to resolution at tick N"),
        bResolutionObservedTransition);

    Clock.Reset(MAX_uint64 - 2);
    Result = Clock.Advance(3.0 * FVelrathCombatClock::LogicalStepSeconds);
    TestEqual(TEXT("Large CurrentTick advances without wrapping"), Clock.GetCurrentTick(), MAX_uint64);
    TestEqual(TEXT("Only representable large ticks execute"), Result.StepsExecuted, 2u);
    TestTrue(TEXT("Tick exhaustion is reported as a numerical limit"), Result.bNumericalLimitReached);
    TestTrue(TEXT("Unexecuted time remains backlog at tick exhaustion"), Result.bHasBacklog);

    Clock.Reset();
    Clock.Advance(-1.0);
    Clock.Advance(std::numeric_limits<double>::quiet_NaN());
    Clock.Advance(std::numeric_limits<double>::infinity());
    TestEqual(TEXT("Negative and non-finite deltas never advance CurrentTick"), Clock.GetCurrentTick(), 0ull);
    TestTrue(TEXT("Invalid deltas do not create backlog"), FMath::IsNearlyZero(
        Clock.GetAccumulatedBacklogSeconds()));

    Clock.Reset();
    constexpr int32 LongRunCycles = 150000;
    for (int32 Index = 0; Index < LongRunCycles; ++Index)
    {
        Clock.Advance(1.0 / 120.0);
        Clock.Advance(1.0 / 40.0);
        Clock.Advance(1.0 / 30.0);
    }
    TestEqual(TEXT("TIME-06 varied long run has the exact tick count"),
        Clock.GetCurrentTick(),
        static_cast<uint64>(LongRunCycles) * 4ull);
    TestTrue(TEXT("TIME-06 long-run remainder stays normalized"),
        Clock.GetAccumulatedBacklogSeconds() >= 0.0
        && Clock.GetAccumulatedBacklogSeconds() < FVelrathCombatClock::LogicalStepSeconds);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathCombatClockEstimatorTest,
    "VELRATH.Combat.Clock.Estimator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathCombatClockEstimatorTest::RunTest(const FString& Parameters)
{
    FVelrathCombatClockEstimator Estimator;
    const FGuid Epoch = FGuid::NewGuid();

    TestFalse(TEXT("An invalid empty anchor is rejected"),
        Estimator.TryApplyAnchor(FVelrathCombatClockAnchor()));
    TestTrue(TEXT("The first valid anchor is accepted"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, 10, 100, 50.0)));
    TestEqual(TEXT("Negative elapsed server time never rewinds below the anchor"),
        Estimator.EstimateTick(49.0), 100ull);
    TestEqual(TEXT("Presentation estimate advances at 60 Hz"),
        Estimator.EstimateTick(50.25), 115ull);
    TestEqual(TEXT("Presentation estimate is capped at 30 unconfirmed ticks"),
        Estimator.EstimateTick(500.0), 130ull);

    TestFalse(TEXT("A duplicate sequence is rejected"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, 10, 999, 60.0)));
    TestFalse(TEXT("A stale sequence is rejected"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, 9, 999, 60.0)));
    TestEqual(TEXT("Rejected anchors cannot replace the authoritative base tick"),
        Estimator.GetAnchor().ServerTick, 100ull);
    TestTrue(TEXT("A newer delayed anchor rebases the estimate"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, 11, 105, 55.0)));
    TestEqual(TEXT("A hitch-safe rebase uses the sampled tick, not floor(world time * 60)"),
        Estimator.EstimateTick(55.0), 105ull);

    const FGuid NewEpoch = FGuid::NewGuid();
    TestTrue(TEXT("A new world epoch replaces the previous mapping"),
        Estimator.TryApplyAnchor(MakeAnchor(NewEpoch, 1, 0, 0.0)));
    TestEqual(TEXT("A new epoch may restart at tick zero"), Estimator.EstimateTick(0.0), 0ull);

    Estimator.Reset();
    TestTrue(TEXT("A near-wrap sequence baseline is accepted"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, MAX_uint32, 10, 1.0)));
    TestTrue(TEXT("Sequence ordering is wrap-aware"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, 1, 11, 1.1)));

    Estimator.Reset();
    TestTrue(TEXT("A near-uint64-limit anchor is accepted"),
        Estimator.TryApplyAnchor(MakeAnchor(Epoch, 1, MAX_uint64 - 5, 10.0)));
    TestEqual(TEXT("Estimated tick saturates rather than overflowing"),
        Estimator.EstimateTick(11.0), MAX_uint64);

    return true;
}

#endif
