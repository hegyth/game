#include "Attributes/VelrathAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"

UVelrathAttributeSet::UVelrathAttributeSet()
    : Health(DefaultHealth)
    , MaxHealth(DefaultMaxHealth)
    , Stamina(DefaultStamina)
    , MaxStamina(DefaultMaxStamina)
{
}

void UVelrathAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UVelrathAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UVelrathAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UVelrathAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UVelrathAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UVelrathAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);
    NewValue = ClampAttributeValue(Attribute, NewValue);
}

void UVelrathAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    NewValue = ClampAttributeValue(Attribute, NewValue);
}

void UVelrathAttributeSet::PostAttributeChange(
    const FGameplayAttribute& Attribute,
    const float OldValue,
    const float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
    {
        SetHealth(NewValue);
    }
    else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
    {
        SetStamina(NewValue);
    }
}

void UVelrathAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetMaxHealth(FMath::Max(GetMaxHealth(), 0.0f));
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
    {
        SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
    }
    else if (Data.EvaluatedData.Attribute == GetMaxStaminaAttribute())
    {
        SetMaxStamina(FMath::Max(GetMaxStamina(), 0.0f));
        SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
    }
}

void UVelrathAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UVelrathAttributeSet, Health, OldHealth);
}

void UVelrathAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UVelrathAttributeSet, MaxHealth, OldMaxHealth);
}

void UVelrathAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UVelrathAttributeSet, Stamina, OldStamina);
}

void UVelrathAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UVelrathAttributeSet, MaxStamina, OldMaxStamina);
}

float UVelrathAttributeSet::ClampAttributeValue(const FGameplayAttribute& Attribute, const float NewValue) const
{
    if (Attribute == GetHealthAttribute())
    {
        return FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }

    if (Attribute == GetMaxHealthAttribute())
    {
        return FMath::Max(NewValue, 0.0f);
    }

    if (Attribute == GetStaminaAttribute())
    {
        return FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
    }

    if (Attribute == GetMaxStaminaAttribute())
    {
        return FMath::Max(NewValue, 0.0f);
    }

    return NewValue;
}
