#include "Combat/VelrathCombatClockSubsystem.h"

#include "Engine/World.h"
#include "Game/VelrathGameState.h"

void UVelrathCombatClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ResetRuntimeState();
}

void UVelrathCombatClockSubsystem::Deinitialize()
{
    ResetRuntimeState();
    CombatStepDelegate.Clear();
    Super::Deinitialize();
}

void UVelrathCombatClockSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    ResetRuntimeState();

    if (InWorld.GetNetMode() != NM_Client)
    {
        ClockEpoch = FGuid::NewGuid();
        bGameplayClockActive = ClockEpoch.IsValid();
        PublishAnchor();
    }
}

void UVelrathCombatClockSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
    ResetRuntimeState();
    Super::OnWorldEndPlay(InWorld);
}

void UVelrathCombatClockSubsystem::Tick(const float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bGameplayClockActive)
    {
        return;
    }

    const FVelrathCombatClockAdvanceResult Result = Clock.Advance(
        static_cast<double>(DeltaTime),
        [this](const uint64 TickIndex)
        {
            CombatStepDelegate.Broadcast(TickIndex);
        });

    LastWorldTickStepCount = Result.StepsExecuted;
    bLastWorldTickHitStepLimit = Result.bHitStepLimit;

    const bool bPreviousOverrun = bOverrun;
    bOverrun = Result.bHasBacklog;
    ConsecutiveBacklogWorldTicks = bOverrun
        ? FMath::Min(ConsecutiveBacklogWorldTicks + 1ull, static_cast<uint64>(MAX_uint32))
        : 0;
    bSustainedBacklog = ConsecutiveBacklogWorldTicks >= SustainedBacklogWorldTicks;

    if (FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f)
    {
        AnchorElapsedSeconds += static_cast<double>(DeltaTime);
    }

    if (bPreviousOverrun != bOverrun)
    {
        AnchorElapsedSeconds = 0.0;
        PublishAnchor();
    }
    else if (AnchorElapsedSeconds >= AnchorPeriodSeconds)
    {
        AnchorElapsedSeconds = FMath::Fmod(AnchorElapsedSeconds, AnchorPeriodSeconds);
        PublishAnchor();
    }
}

bool UVelrathCombatClockSubsystem::IsTickable() const
{
    return bGameplayClockActive && !IsTemplate();
}

TStatId UVelrathCombatClockSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVelrathCombatClockSubsystem, STATGROUP_Tickables);
}

bool UVelrathCombatClockSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVelrathCombatClockSubsystem::ResetRuntimeState()
{
    Clock.Reset();
    ClockEpoch.Invalidate();
    AnchorElapsedSeconds = 0.0;
    LastWorldTickStepCount = 0;
    ConsecutiveBacklogWorldTicks = 0;
    bLastWorldTickHitStepLimit = false;
    bSustainedBacklog = false;
    bOverrun = false;
    bGameplayClockActive = false;
}

void UVelrathCombatClockSubsystem::PublishAnchor()
{
    UWorld* World = GetWorld();
    AVelrathGameState* GameState = World != nullptr ? World->GetGameState<AVelrathGameState>() : nullptr;
    if (GameState != nullptr)
    {
        GameState->PublishCombatClockAnchor(ClockEpoch, Clock.GetCurrentTick());
    }
}
