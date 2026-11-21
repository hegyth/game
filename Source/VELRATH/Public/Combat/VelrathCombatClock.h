#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct VELRATH_API FVelrathCombatClockAdvanceResult
{
    uint32 StepsExecuted = 0;
    bool bHitStepLimit = false;
    bool bHasBacklog = false;
    bool bNumericalLimitReached = false;
};

/** Pure fixed-step combat timekeeper. It has no UObject, Actor, GAS, or Blueprint dependency. */
class VELRATH_API FVelrathCombatClock
{
public:
    static constexpr uint32 TicksPerSecond = 60;
    static constexpr uint32 MaxStepsPerWorldTick = 8;
    static constexpr double LogicalStepSeconds = 1.0 / static_cast<double>(TicksPerSecond);

    FVelrathCombatClockAdvanceResult Advance(double DeltaTimeSeconds);
    FVelrathCombatClockAdvanceResult Advance(
        double DeltaTimeSeconds,
        TFunctionRef<void(uint64)> OnStep);

    void Reset(uint64 InitialTick = 0);

    uint64 GetCurrentTick() const { return CurrentTick; }
    double GetAccumulatedBacklogSeconds() const;
    bool HasCompleteStepBacklog() const;
    bool IsAtTickLimit() const { return CurrentTick == MAX_uint64; }

    static bool IsTickInHalfOpenInterval(uint64 Tick, uint64 StartTick, uint64 EndTick);
    static bool TryGetLocalTick(uint64 CurrentTick, uint64 StartTick, uint64& OutLocalTick);

private:
    FVelrathCombatClockAdvanceResult AdvanceInternal(
        double DeltaTimeSeconds,
        TFunctionRef<void(uint64)> OnStep);
    void AccumulateDelta(double DeltaTimeSeconds);

    uint64 CurrentTick = 0;
    double AccumulatedTickUnits = 0.0;
    double AccumulationCompensation = 0.0;
    bool bNumericalLimitReached = false;
};
