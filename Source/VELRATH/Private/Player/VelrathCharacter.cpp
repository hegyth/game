#include "Player/VelrathCharacter.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

AVelrathCharacter::AVelrathCharacter()
{
    bReplicates = true;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AVelrathCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AVelrathCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
}

void AVelrathCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilityActorInfo();
}

void AVelrathCharacter::InitializeAbilityActorInfo()
{
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

