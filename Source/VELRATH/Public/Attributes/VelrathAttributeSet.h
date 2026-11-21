#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "VelrathAttributeSet.generated.h"

UCLASS()
class VELRATH_API UVelrathAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UVelrathAttributeSet();

    ATTRIBUTE_ACCESSORS_BASIC(UVelrathAttributeSet, Health)
    ATTRIBUTE_ACCESSORS_BASIC(UVelrathAttributeSet, MaxHealth)
    ATTRIBUTE_ACCESSORS_BASIC(UVelrathAttributeSet, Stamina)
    ATTRIBUTE_ACCESSORS_BASIC(UVelrathAttributeSet, MaxStamina)

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
    FGameplayAttributeData MaxHealth;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Stamina")
    FGameplayAttributeData Stamina;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Stamina")
    FGameplayAttributeData MaxStamina;

    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldHealth);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

    UFUNCTION()
    void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

    UFUNCTION()
    void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

private:
    float ClampAttributeValue(const FGameplayAttribute& Attribute, float NewValue) const;
};
