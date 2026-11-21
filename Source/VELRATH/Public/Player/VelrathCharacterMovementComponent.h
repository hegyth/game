#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VelrathCharacterMovementComponent.generated.h"

UCLASS()
class VELRATH_API UVelrathCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UVelrathCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    void SetWantsToSprint(bool bNewWantsToSprint);
    bool WantsToSprint() const { return bWantsToSprint; }

    virtual float GetMaxSpeed() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
    uint8 bWantsToSprint : 1;
};
