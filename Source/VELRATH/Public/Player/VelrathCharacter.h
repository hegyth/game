#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VelrathCharacter.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class VELRATH_API AVelrathCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AVelrathCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

private:
    void InitializeAbilityActorInfo();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UAttributeSet> AttributeSet;
};

