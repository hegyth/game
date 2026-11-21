#include "Player/VelrathCharacterMovementComponent.h"

#include "Player/VelrathCharacter.h"

namespace
{
class FSavedMove_Velrath final : public FSavedMove_Character
{
public:
    using Super = FSavedMove_Character;

    uint8 bSavedWantsToSprint : 1;

    virtual void Clear() override
    {
        Super::Clear();
        bSavedWantsToSprint = false;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Result = Super::GetCompressedFlags();
        if (bSavedWantsToSprint)
        {
            Result |= FLAG_Custom_0;
        }
        return Result;
    }

    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
    {
        const FSavedMove_Velrath* NewVelrathMove = static_cast<const FSavedMove_Velrath*>(NewMove.Get());
        if (bSavedWantsToSprint != NewVelrathMove->bSavedWantsToSprint)
        {
            return false;
        }

        return Super::CanCombineWith(NewMove, Character, MaxDelta);
    }

    virtual void SetMoveFor(
        ACharacter* Character,
        float InDeltaTime,
        const FVector& NewAcceleration,
        FNetworkPredictionData_Client_Character& ClientData) override
    {
        Super::SetMoveFor(Character, InDeltaTime, NewAcceleration, ClientData);

        const UVelrathCharacterMovementComponent* MovementComponent =
            Cast<UVelrathCharacterMovementComponent>(Character->GetCharacterMovement());
        bSavedWantsToSprint = MovementComponent != nullptr && MovementComponent->WantsToSprint();
    }

    virtual void PrepMoveFor(ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);

        UVelrathCharacterMovementComponent* MovementComponent =
            Cast<UVelrathCharacterMovementComponent>(Character->GetCharacterMovement());
        if (MovementComponent != nullptr)
        {
            MovementComponent->SetWantsToSprint(bSavedWantsToSprint);
        }
    }
};

class FNetworkPredictionData_Client_Velrath final : public FNetworkPredictionData_Client_Character
{
public:
    using Super = FNetworkPredictionData_Client_Character;

    explicit FNetworkPredictionData_Client_Velrath(const UCharacterMovementComponent& ClientMovement)
        : Super(ClientMovement)
    {
    }

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_Velrath());
    }
};
}

UVelrathCharacterMovementComponent::UVelrathCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , bWantsToSprint(false)
{
}

void UVelrathCharacterMovementComponent::SetWantsToSprint(const bool bNewWantsToSprint)
{
    bWantsToSprint = bNewWantsToSprint;
}

float UVelrathCharacterMovementComponent::GetMaxSpeed() const
{
    if (bWantsToSprint)
    {
        const AVelrathCharacter* VelrathCharacter = Cast<AVelrathCharacter>(CharacterOwner);
        if (VelrathCharacter != nullptr)
        {
            return VelrathCharacter->GetSprintSpeed();
        }
    }

    return Super::GetMaxSpeed();
}

void UVelrathCharacterMovementComponent::UpdateFromCompressedFlags(const uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UVelrathCharacterMovementComponent::GetPredictionData_Client() const
{
    if (ClientPredictionData == nullptr)
    {
        UVelrathCharacterMovementComponent* MutableThis =
            const_cast<UVelrathCharacterMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Velrath(*this);
    }

    return ClientPredictionData;
}
