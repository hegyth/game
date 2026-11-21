#pragma once

#include "CoreMinimal.h"
#include "VelrathCombatClockAnchor.generated.h"

USTRUCT()
struct VELRATH_API FVelrathCombatClockAnchor
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid ClockEpoch;

    UPROPERTY()
    uint32 AnchorSequence = 0;

    UPROPERTY()
    uint64 ServerTick = 0;

    UPROPERTY()
    double SampledServerWorldTimeSeconds = 0.0;

    bool IsValid() const;
};

/** Pure latest-anchor ordering and bounded presentation estimate helper. */
class VELRATH_API FVelrathCombatClockEstimator
{
public:
    static constexpr uint64 MaxExtrapolationTicks = 30;

    bool TryApplyAnchor(const FVelrathCombatClockAnchor& Candidate);
    uint64 EstimateTick(double EstimatedServerWorldTimeSeconds) const;
    void Reset();

    bool HasAnchor() const { return bHasAnchor; }
    const FVelrathCombatClockAnchor& GetAnchor() const { return Anchor; }

    static bool IsSequenceNewer(uint32 Candidate, uint32 Current);

private:
    FVelrathCombatClockAnchor Anchor;
    bool bHasAnchor = false;
};
