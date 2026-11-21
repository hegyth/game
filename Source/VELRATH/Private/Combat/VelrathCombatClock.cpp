#include "Combat/VelrathCombatClock.h"

#include <cfloat>

namespace
{
constexpr double StepExtractionTolerance = 1.0e-9;
}

FVelrathCombatClockAdvanceResult FVelrathCombatClock::Advance(const double DeltaTimeSeconds)
{
    return AdvanceInternal(DeltaTimeSeconds, [](uint64) {});
}

FVelrathCombatClockAdvanceResult FVelrathCombatClock::Advance(
    const double DeltaTimeSeconds,
    TFunctionRef<void(uint64)> OnStep)
{
    return AdvanceInternal(DeltaTimeSeconds, OnStep);
}

FVelrathCombatClockAdvanceResult FVelrathCombatClock::AdvanceInternal(
    const double DeltaTimeSeconds,
    TFunctionRef<void(uint64)> OnStep)
{
    AccumulateDelta(DeltaTimeSeconds);

    FVelrathCombatClockAdvanceResult Result;
    while (Result.StepsExecuted < MaxStepsPerWorldTick
        && AccumulatedTickUnits + StepExtractionTolerance >= 1.0)
    {
        if (CurrentTick == MAX_uint64)
        {
            bNumericalLimitReached = true;
            break;
        }

        AccumulatedTickUnits -= 1.0;
        if (AccumulatedTickUnits < 0.0 && AccumulatedTickUnits >= -StepExtractionTolerance)
        {
            AccumulatedTickUnits = 0.0;
        }

        ++CurrentTick;
        ++Result.StepsExecuted;
        OnStep(CurrentTick);
    }

    Result.bHitStepLimit = Result.StepsExecuted == MaxStepsPerWorldTick;
    Result.bHasBacklog = HasCompleteStepBacklog();
    Result.bNumericalLimitReached = bNumericalLimitReached;
    return Result;
}

void FVelrathCombatClock::AccumulateDelta(const double DeltaTimeSeconds)
{
    if (!FMath::IsFinite(DeltaTimeSeconds) || DeltaTimeSeconds <= 0.0)
    {
        return;
    }

    if (DeltaTimeSeconds > DBL_MAX / static_cast<double>(TicksPerSecond))
    {
        AccumulatedTickUnits = DBL_MAX;
        AccumulationCompensation = 0.0;
        bNumericalLimitReached = true;
        return;
    }

    const double TickUnits = DeltaTimeSeconds * static_cast<double>(TicksPerSecond);
    const double CompensatedDelta = TickUnits - AccumulationCompensation;
    const double NewAccumulatedTickUnits = AccumulatedTickUnits + CompensatedDelta;
    if (!FMath::IsFinite(NewAccumulatedTickUnits))
    {
        AccumulatedTickUnits = DBL_MAX;
        AccumulationCompensation = 0.0;
        bNumericalLimitReached = true;
        return;
    }

    AccumulationCompensation = (NewAccumulatedTickUnits - AccumulatedTickUnits) - CompensatedDelta;
    AccumulatedTickUnits = NewAccumulatedTickUnits;
}

void FVelrathCombatClock::Reset(const uint64 InitialTick)
{
    CurrentTick = InitialTick;
    AccumulatedTickUnits = 0.0;
    AccumulationCompensation = 0.0;
    bNumericalLimitReached = false;
}

double FVelrathCombatClock::GetAccumulatedBacklogSeconds() const
{
    if (AccumulatedTickUnits >= DBL_MAX / LogicalStepSeconds)
    {
        return DBL_MAX;
    }

    return FMath::Max(0.0, AccumulatedTickUnits * LogicalStepSeconds);
}

bool FVelrathCombatClock::HasCompleteStepBacklog() const
{
    return AccumulatedTickUnits + StepExtractionTolerance >= 1.0;
}

bool FVelrathCombatClock::IsTickInHalfOpenInterval(
    const uint64 Tick,
    const uint64 StartTick,
    const uint64 EndTick)
{
    return StartTick <= Tick && Tick < EndTick;
}

bool FVelrathCombatClock::TryGetLocalTick(
    const uint64 InCurrentTick,
    const uint64 StartTick,
    uint64& OutLocalTick)
{
    if (InCurrentTick < StartTick)
    {
        OutLocalTick = 0;
        return false;
    }

    OutLocalTick = InCurrentTick - StartTick;
    return true;
}
