#include "Game/VelrathGameState.h"

#include "Net/UnrealNetwork.h"

AVelrathGameState::AVelrathGameState()
{
    bReplicates = true;
}

void AVelrathGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVelrathGameState, CombatClockAnchor);
}

bool AVelrathGameState::PublishCombatClockAnchor(
    const FGuid& ClockEpoch,
    const uint64 ServerTick)
{
    if (!HasAuthority() || !ClockEpoch.IsValid())
    {
        return false;
    }

    CombatClockAnchor.ClockEpoch = ClockEpoch;
    ++CombatClockAnchor.AnchorSequence;
    if (CombatClockAnchor.AnchorSequence == 0)
    {
        ++CombatClockAnchor.AnchorSequence;
    }
    CombatClockAnchor.ServerTick = ServerTick;
    CombatClockAnchor.SampledServerWorldTimeSeconds = GetServerWorldTimeSeconds();
    CombatClockEstimator.TryApplyAnchor(CombatClockAnchor);
    ForceNetUpdate();
    return true;
}

uint64 AVelrathGameState::GetEstimatedCombatTick() const
{
    return CombatClockEstimator.EstimateTick(GetServerWorldTimeSeconds());
}

void AVelrathGameState::OnRep_CombatClockAnchor()
{
    CombatClockEstimator.TryApplyAnchor(CombatClockAnchor);
}
