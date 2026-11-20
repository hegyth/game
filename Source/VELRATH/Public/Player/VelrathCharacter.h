#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VelrathCharacter.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UCameraComponent;
class USpringArmComponent;

UCLASS()
class VELRATH_API AVelrathCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AVelrathCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void PawnClientRestart() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

    USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    UCameraComponent* GetFollowCamera() const { return FollowCamera; }
    float GetInitialCameraPitch() const { return InitialCameraPitch; }

    float GetSprintSpeed() const { return SprintSpeed; }
    float GetBackwardMovementMultiplier() const { return BackwardMovementMultiplier; }
    float GetAttackMovementMultiplier() const { return AttackMovementMultiplier; }
    float GetBlockMovementMultiplier() const { return BlockMovementMultiplier; }

private:
    void InitializeAbilityActorInfo();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    // Starting framing hypothesis. Applied to the local PlayerController's ControlRotation after possession.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
    float InitialCameraPitch = -20.0f;

    // Stored for the dedicated input/combat-state tasks. FND-002 does not apply these values.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
    float SprintSpeed = 660.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float BackwardMovementMultiplier = 0.85f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float AttackMovementMultiplier = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float BlockMovementMultiplier = 0.60f;
};

