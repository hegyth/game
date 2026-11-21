#pragma once

#include "CoreMinimal.h"
#include "Combat/VelrathCombatClockAnchor.h"
#include "GameFramework/GameStateBase.h"
#include "VelrathGameState.generated.h"

UCLASS()
class VELRATH_API AVelrathGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AVelrathGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool PublishCombatClockAnchor(const FGuid& ClockEpoch, uint64 ServerTick);
    const FVelrathCombatClockAnchor& GetCombatClockAnchor() const { return CombatClockAnchor; }
    uint64 GetEstimatedCombatTick() const;

protected:
    UFUNCTION()
    void OnRep_CombatClockAnchor();

private:
    UPROPERTY(ReplicatedUsing = OnRep_CombatClockAnchor)
    FVelrathCombatClockAnchor CombatClockAnchor;

    FVelrathCombatClockEstimator CombatClockEstimator;
};
