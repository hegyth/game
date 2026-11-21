#include "Combat/VelrathCombatClockAnchor.h"

#include "Combat/VelrathCombatClock.h"

bool FVelrathCombatClockAnchor::IsValid() const
{
    return ClockEpoch.IsValid()
        && AnchorSequence != 0
        && FMath::IsFinite(SampledServerWorldTimeSeconds)
        && SampledServerWorldTimeSeconds >= 0.0;
}

bool FVelrathCombatClockEstimator::TryApplyAnchor(const FVelrathCombatClockAnchor& Candidate)
{
    if (!Candidate.IsValid())
    {
        return false;
    }

    if (bHasAnchor
        && Candidate.ClockEpoch == Anchor.ClockEpoch
        && !IsSequenceNewer(Candidate.AnchorSequence, Anchor.AnchorSequence))
    {
        return false;
    }

    Anchor = Candidate;
    bHasAnchor = true;
    return true;
}

uint64 FVelrathCombatClockEstimator::EstimateTick(
    const double EstimatedServerWorldTimeSeconds) const
{
    if (!bHasAnchor || !FMath::IsFinite(EstimatedServerWorldTimeSeconds))
    {
        return bHasAnchor ? Anchor.ServerTick : 0;
    }

    const double ElapsedSeconds = FMath::Max(
        0.0,
        EstimatedServerWorldTimeSeconds - Anchor.SampledServerWorldTimeSeconds);
    const double RawElapsedTicks = FMath::FloorToDouble(
        ElapsedSeconds * static_cast<double>(FVelrathCombatClock::TicksPerSecond));
    const uint64 ElapsedTicks = RawElapsedTicks >= static_cast<double>(MaxExtrapolationTicks)
        ? MaxExtrapolationTicks
        : static_cast<uint64>(RawElapsedTicks);

    if (Anchor.ServerTick > MAX_uint64 - ElapsedTicks)
    {
        return MAX_uint64;
    }

    return Anchor.ServerTick + ElapsedTicks;
}

void FVelrathCombatClockEstimator::Reset()
{
    Anchor = FVelrathCombatClockAnchor();
    bHasAnchor = false;
}

bool FVelrathCombatClockEstimator::IsSequenceNewer(
    const uint32 Candidate,
    const uint32 Current)
{
    return static_cast<int32>(Candidate - Current) > 0;
}
