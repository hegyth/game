#include "Abilities/VelrathAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

namespace
{
struct FExpectedInitialModifier
{
    FGameplayAttribute Attribute;
};

bool ValidateModifier(
    const FGameplayModifierInfo& Modifier,
    const FExpectedInitialModifier& Expected,
    const int32 Index,
    float& OutMagnitude,
    FText& OutError)
{
    if (Modifier.Attribute != Expected.Attribute)
    {
        OutError = FText::Format(
            NSLOCTEXT("VelrathAbilitySet", "WrongAttribute", "Modifier {0} must target {1}."),
            Index,
            FText::FromString(Expected.Attribute.GetName()));
        return false;
    }

    if (Modifier.ModifierOp != EGameplayModOp::Override)
    {
        OutError = FText::Format(
            NSLOCTEXT("VelrathAbilitySet", "WrongOperation", "Modifier {0} must use Override."),
            Index);
        return false;
    }

    if (!Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, OutMagnitude))
    {
        OutError = FText::Format(
            NSLOCTEXT("VelrathAbilitySet", "DynamicMagnitude", "Modifier {0} must use a static magnitude."),
            Index);
        return false;
    }

    if (!FMath::IsFinite(OutMagnitude) || OutMagnitude < 0.0f)
    {
        OutError = FText::Format(
            NSLOCTEXT("VelrathAbilitySet", "WrongMagnitude", "Modifier {0} must have a finite, non-negative magnitude."),
            Index);
        return false;
    }

    return true;
}
}

bool FVelrathAbilitySetGrantedHandles::IsGrantCompleteFor(
    const UAbilitySystemComponent* InAbilitySystemComponent,
    const UVelrathAbilitySet* InAbilitySet) const
{
    return bGrantComplete &&
        AbilitySystemComponent.Get() == InAbilitySystemComponent &&
        (InAbilitySet == nullptr || GrantingAbilitySet.Get() == InAbilitySet);
}

void FVelrathAbilitySetGrantedHandles::Reset()
{
    AbilitySystemComponent.Reset();
    GrantingAbilitySet.Reset();
    AbilitySpecHandles.Reset();
    bGrantComplete = false;
}

EDataValidationResult UVelrathAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult SuperResult = Super::IsDataValid(Context);

    FText Error;
    if (!ValidateInitialAttributesEffect(Error))
    {
        Context.AddError(Error);
        return EDataValidationResult::Invalid;
    }

    if (!ValidateGrantedAbilities(Error))
    {
        Context.AddError(Error);
        return EDataValidationResult::Invalid;
    }

    return SuperResult == EDataValidationResult::Invalid
        ? EDataValidationResult::Invalid
        : EDataValidationResult::Valid;
}

bool UVelrathAbilitySet::ValidateInitialAttributesEffect(FText& OutError) const
{
    if (InitialAttributesEffect == nullptr)
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "MissingInitialEffect",
            "Initial Attributes Effect must reference a Gameplay Effect class.");
        return false;
    }

    return ValidateInitialAttributesEffect(InitialAttributesEffect->GetDefaultObject<UGameplayEffect>(), OutError);
}

bool UVelrathAbilitySet::ValidateInitialAttributesEffect(
    const UGameplayEffect* GameplayEffect,
    FText& OutError)
{
    if (GameplayEffect == nullptr)
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "NullInitialEffect",
            "Initial Attributes Effect could not be loaded.");
        return false;
    }

    if (GameplayEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "WrongDuration",
            "Initial Attributes Effect must use the Instant duration policy.");
        return false;
    }

    const TStaticArray<FExpectedInitialModifier, 4> ExpectedModifiers = {
        FExpectedInitialModifier{UVelrathAttributeSet::GetMaxHealthAttribute()},
        FExpectedInitialModifier{UVelrathAttributeSet::GetHealthAttribute()},
        FExpectedInitialModifier{UVelrathAttributeSet::GetMaxStaminaAttribute()},
        FExpectedInitialModifier{UVelrathAttributeSet::GetStaminaAttribute()},
    };

    if (GameplayEffect->Modifiers.Num() != ExpectedModifiers.Num())
    {
        OutError = FText::Format(
            NSLOCTEXT("VelrathAbilitySet", "WrongModifierCount", "Initial Attributes Effect must contain exactly {0} modifiers."),
            ExpectedModifiers.Num());
        return false;
    }

    TStaticArray<float, 4> Magnitudes;
    for (int32 Index = 0; Index < ExpectedModifiers.Num(); ++Index)
    {
        if (!ValidateModifier(
            GameplayEffect->Modifiers[Index],
            ExpectedModifiers[Index],
            Index,
            Magnitudes[Index],
            OutError))
        {
            return false;
        }
    }

    if (Magnitudes[1] > Magnitudes[0])
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "HealthAboveMaximum",
            "Initial Health must not exceed initial MaxHealth.");
        return false;
    }

    if (Magnitudes[3] > Magnitudes[2])
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "StaminaAboveMaximum",
            "Initial Stamina must not exceed initial MaxStamina.");
        return false;
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UVelrathAbilitySet::ValidateGrantedAbilities(FText& OutError) const
{
    TSet<const UClass*> UniqueAbilityClasses;
    for (int32 Index = 0; Index < GrantedAbilities.Num(); ++Index)
    {
        const UClass* AbilityClass = GrantedAbilities[Index].Get();
        if (AbilityClass == nullptr)
        {
            OutError = FText::Format(
                NSLOCTEXT("VelrathAbilitySet", "NullAbility", "Granted Ability entry {0} is null."),
                Index);
            return false;
        }

        if (!AbilityClass->IsChildOf(UGameplayAbility::StaticClass()) ||
            AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            OutError = FText::Format(
                NSLOCTEXT("VelrathAbilitySet", "InvalidAbilityClass", "Granted Ability entry {0} must reference a concrete Gameplay Ability class."),
                Index);
            return false;
        }

        if (UniqueAbilityClasses.Contains(AbilityClass))
        {
            OutError = FText::Format(
                NSLOCTEXT("VelrathAbilitySet", "DuplicateAbility", "Granted Ability entry {0} duplicates {1}."),
                Index,
                FText::FromString(AbilityClass->GetPathName()));
            return false;
        }

        UniqueAbilityClasses.Add(AbilityClass);
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UVelrathAbilitySet::GiveToAbilitySystem(
    UAbilitySystemComponent& AbilitySystemComponent,
    FVelrathAbilitySetGrantedHandles& OutGrantedHandles,
    FText& OutError) const
{
    AActor* OwnerActor = AbilitySystemComponent.GetOwnerActor();
    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "GrantRequiresAuthority",
            "Gameplay Abilities can only be granted by an authoritative Ability System Component owner.");
        return false;
    }

    if (!AbilitySystemComponent.AbilityActorInfo.IsValid() ||
        AbilitySystemComponent.GetAvatarActor() == nullptr)
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "ActorInfoNotReady",
            "Ability Actor Info must be initialized before granting Gameplay Abilities.");
        return false;
    }

    if (OutGrantedHandles.IsGrantCompleteFor(&AbilitySystemComponent, this))
    {
        OutError = FText::GetEmpty();
        return true;
    }

    if (OutGrantedHandles.HasGrantState())
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "HandlesBelongToAnotherAsc",
            "Granted handles already belong to another Ability System Component.");
        return false;
    }

    if (!ValidateGrantedAbilities(OutError))
    {
        return false;
    }

    OutGrantedHandles.AbilitySystemComponent = &AbilitySystemComponent;
    OutGrantedHandles.GrantingAbilitySet = const_cast<UVelrathAbilitySet*>(this);
    for (const TSubclassOf<UGameplayAbility>& AbilityClass : GrantedAbilities)
    {
        const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent.GiveAbility(
            FGameplayAbilitySpec(AbilityClass, 1));
        if (!Handle.IsValid())
        {
            for (const FGameplayAbilitySpecHandle& GrantedHandle : OutGrantedHandles.AbilitySpecHandles)
            {
                AbilitySystemComponent.ClearAbility(GrantedHandle);
            }
            OutGrantedHandles.Reset();
            OutError = FText::Format(
                NSLOCTEXT("VelrathAbilitySet", "GrantFailed", "GAS rejected the grant for {0}."),
                FText::FromString(AbilityClass->GetPathName()));
            return false;
        }

        OutGrantedHandles.AbilitySpecHandles.Add(Handle);
    }

    OutGrantedHandles.bGrantComplete = true;
    OutError = FText::GetEmpty();
    return true;
}

bool UVelrathAbilitySet::TakeFromAbilitySystem(
    FVelrathAbilitySetGrantedHandles& GrantedHandles,
    FText& OutError) const
{
    if (!GrantedHandles.HasGrantState())
    {
        OutError = FText::GetEmpty();
        return true;
    }

    UAbilitySystemComponent* AbilitySystemComponent = GrantedHandles.AbilitySystemComponent.Get();
    AActor* OwnerActor = AbilitySystemComponent != nullptr ? AbilitySystemComponent->GetOwnerActor() : nullptr;
    if (GrantedHandles.GrantingAbilitySet.Get() != this)
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "RemovalSetMismatch",
            "Granted handles can only be removed by the Ability Set that issued them.");
        return false;
    }

    if (AbilitySystemComponent == nullptr || OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        OutError = NSLOCTEXT(
            "VelrathAbilitySet",
            "RemovalRequiresAuthority",
            "Gameplay Abilities can only be removed from their original authoritative Ability System Component.");
        return false;
    }

    for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles.AbilitySpecHandles)
    {
        if (Handle.IsValid())
        {
            AbilitySystemComponent->ClearAbility(Handle);
        }
    }

    GrantedHandles.Reset();
    OutError = FText::GetEmpty();
    return true;
}
