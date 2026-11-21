#pragma once

#include "CoreMinimal.h"
#include "Combat/VelrathCombatClock.h"
#include "Subsystems/WorldSubsystem.h"
#include "VelrathCombatClockSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FVelrathCombatStepDelegate, uint64);

UCLASS()
class VELRATH_API UVelrathCombatClockSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void OnWorldEndPlay(UWorld& InWorld) override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;

    uint64 GetCurrentTick() const { return Clock.GetCurrentTick(); }
    double GetAccumulatedBacklogSeconds() const { return Clock.GetAccumulatedBacklogSeconds(); }
    uint32 GetLastWorldTickStepCount() const { return LastWorldTickStepCount; }
    bool DidLastWorldTickHitStepLimit() const { return bLastWorldTickHitStepLimit; }
    bool HasSustainedBacklog() const { return bSustainedBacklog; }
    bool IsOverrun() const { return bOverrun; }
    bool IsGameplayClockActive() const { return bGameplayClockActive; }
    const FGuid& GetClockEpoch() const { return ClockEpoch; }
    FVelrathCombatStepDelegate& OnCombatStep() { return CombatStepDelegate; }

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
    void ResetRuntimeState();
    void PublishAnchor();

    static constexpr double AnchorPeriodSeconds = 0.1;
    static constexpr uint32 SustainedBacklogWorldTicks = 2;

    FVelrathCombatClock Clock;
    FGuid ClockEpoch;
    FVelrathCombatStepDelegate CombatStepDelegate;
    double AnchorElapsedSeconds = 0.0;
    uint32 LastWorldTickStepCount = 0;
    uint32 ConsecutiveBacklogWorldTicks = 0;
    bool bLastWorldTickHitStepLimit = false;
    bool bSustainedBacklog = false;
    bool bOverrun = false;
    bool bGameplayClockActive = false;
};
