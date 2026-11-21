#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "VelrathAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UVelrathAbilitySet;

USTRUCT()
struct VELRATH_API FVelrathAbilitySetGrantedHandles
{
    GENERATED_BODY()

public:
    bool IsGrantCompleteFor(
        const UAbilitySystemComponent* InAbilitySystemComponent,
        const UVelrathAbilitySet* InAbilitySet = nullptr) const;
    bool HasGrantState() const
    {
        return bGrantComplete || AbilitySystemComponent.IsValid() || GrantingAbilitySet.IsValid() || !AbilitySpecHandles.IsEmpty();
    }
    int32 Num() const { return AbilitySpecHandles.Num(); }
    const TArray<FGameplayAbilitySpecHandle>& GetAbilitySpecHandles() const { return AbilitySpecHandles; }

private:
    friend class UVelrathAbilitySet;

    void Reset();

    UPROPERTY(Transient)
    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(Transient)
    TWeakObjectPtr<UVelrathAbilitySet> GrantingAbilitySet;

    UPROPERTY(Transient)
    TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

    UPROPERTY(Transient)
    bool bGrantComplete = false;
};

UCLASS(BlueprintType, Const)
class VELRATH_API UVelrathAbilitySet : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

    TSubclassOf<UGameplayEffect> GetInitialAttributesEffect() const { return InitialAttributesEffect; }
    const TArray<TSubclassOf<UGameplayAbility>>& GetGrantedAbilities() const { return GrantedAbilities; }

    bool ValidateInitialAttributesEffect(FText& OutError) const;
    static bool ValidateInitialAttributesEffect(const UGameplayEffect* GameplayEffect, FText& OutError);
    bool ValidateGrantedAbilities(FText& OutError) const;

    bool GiveToAbilitySystem(
        UAbilitySystemComponent& AbilitySystemComponent,
        FVelrathAbilitySetGrantedHandles& OutGrantedHandles,
        FText& OutError) const;
    bool TakeFromAbilitySystem(FVelrathAbilitySetGrantedHandles& GrantedHandles, FText& OutError) const;

#if WITH_EDITOR
    void SetInitialAttributesEffect(TSubclassOf<UGameplayEffect> InEffect) { InitialAttributesEffect = InEffect; }
    void SetGrantedAbilities(const TArray<TSubclassOf<UGameplayAbility>>& InAbilities) { GrantedAbilities = InAbilities; }
#endif

private:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Startup", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UGameplayEffect> InitialAttributesEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Startup", meta = (AllowPrivateAccess = "true"))
    TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};
