#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/VelrathAbilitySet.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"
#include "Player/VelrathCharacter.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/VelrathTestGameplayAbilities.h"

namespace
{
constexpr TCHAR StartupAssetPath[] = TEXT("/Game/Abilities/DA_VelrathSharedStartup.DA_VelrathSharedStartup");

UGameplayEffect* MakeValidInitialEffect()
{
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;

    const auto AddModifier = [Effect](const FGameplayAttribute& Attribute, const float Magnitude)
    {
        FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
        Modifier.Attribute = Attribute;
        Modifier.ModifierOp = EGameplayModOp::Override;
        Modifier.ModifierMagnitude = FScalableFloat(Magnitude);
    };

    AddModifier(UVelrathAttributeSet::GetMaxHealthAttribute(), 1000.0f);
    AddModifier(UVelrathAttributeSet::GetHealthAttribute(), 1000.0f);
    AddModifier(UVelrathAttributeSet::GetMaxStaminaAttribute(), 100.0f);
    AddModifier(UVelrathAttributeSet::GetStaminaAttribute(), 100.0f);
    return Effect;
}


UVelrathAbilitySet* MakeAbilitySet(
    UVelrathAbilitySet& SourceAsset,
    const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses)
{
    UVelrathAbilitySet* AbilitySet = NewObject<UVelrathAbilitySet>(GetTransientPackage());
    AbilitySet->SetInitialAttributesEffect(SourceAsset.GetInitialAttributesEffect());
    AbilitySet->SetGrantedAbilities(AbilityClasses);
    return AbilitySet;
}

int32 CountAbilitySpecs(const UAbilitySystemComponent& AbilitySystemComponent)
{
    return AbilitySystemComponent.GetActivatableAbilities().Num();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathAbilitySetContentTest,
    "VELRATH.GAS.Startup.Content",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathAbilitySetContentTest::RunTest(const FString& Parameters)
{
    UVelrathAbilitySet* StartupAsset = LoadObject<UVelrathAbilitySet>(nullptr, StartupAssetPath);
    TestNotNull(TEXT("Shared startup Primary Data Asset loads"), StartupAsset);
    if (StartupAsset == nullptr)
    {
        return false;
    }

    TestEqual(
        TEXT("Shared startup asset uses the UVelrathAbilitySet schema"),
        StartupAsset->GetClass(),
        UVelrathAbilitySet::StaticClass());

    FText ValidationError;
    TestTrue(
        TEXT("Shared startup asset validates its initial Gameplay Effect"),
        StartupAsset->ValidateInitialAttributesEffect(ValidationError));
    if (!ValidationError.IsEmpty())
    {
        AddError(ValidationError.ToString());
    }

    FDataValidationContext ValidationContext;
    TestEqual(
        TEXT("Shared startup asset passes Unreal data validation"),
        StartupAsset->IsDataValid(ValidationContext),
        EDataValidationResult::Valid);
    TestEqual(TEXT("Shared startup asset has no validation errors"), ValidationContext.GetNumErrors(), 0u);

    TestEqual(
        TEXT("Production startup asset does not contain placeholder combat abilities"),
        StartupAsset->GetGrantedAbilities().Num(),
        0);

    UVelrathAbilitySet* EmptyAbilitySet = MakeAbilitySet(*StartupAsset, {});
    TestTrue(
        TEXT("An empty startup ability list is valid"),
        EmptyAbilitySet->ValidateGrantedAbilities(ValidationError));

    UVelrathAbilitySet* ValidAbilitySet = MakeAbilitySet(
        *StartupAsset,
        {UVelrathGrantTestAbilityOne::StaticClass(), UVelrathGrantTestAbilityTwo::StaticClass()});
    TestTrue(
        TEXT("Multiple distinct Gameplay Ability classes validate"),
        ValidAbilitySet->ValidateGrantedAbilities(ValidationError));

    UVelrathAbilitySet* NullAbilitySet = MakeAbilitySet(
        *StartupAsset,
        {UVelrathGrantTestAbilityOne::StaticClass(), nullptr});
    TestFalse(
        TEXT("A null Gameplay Ability entry is rejected"),
        NullAbilitySet->ValidateGrantedAbilities(ValidationError));

    UVelrathAbilitySet* DuplicateAbilitySet = MakeAbilitySet(
        *StartupAsset,
        {UVelrathGrantTestAbilityOne::StaticClass(), UVelrathGrantTestAbilityOne::StaticClass()});
    TestFalse(
        TEXT("A duplicate Gameplay Ability entry is rejected"),
        DuplicateAbilitySet->ValidateGrantedAbilities(ValidationError));

    FDataValidationContext DuplicateValidationContext;
    TestEqual(
        TEXT("Unreal data validation rejects duplicate ability entries"),
        DuplicateAbilitySet->IsDataValid(DuplicateValidationContext),
        EDataValidationResult::Invalid);

    const TSubclassOf<UGameplayEffect> EffectClass = StartupAsset->GetInitialAttributesEffect();
    TestNotNull(TEXT("Shared startup asset references GE_VelrathInitialAttributes"), EffectClass.Get());
    if (EffectClass == nullptr)
    {
        return false;
    }

    TestEqual(
        TEXT("Initial Gameplay Effect has the approved asset name"),
        EffectClass->ClassGeneratedBy != nullptr ? EffectClass->ClassGeneratedBy->GetName() : FString(),
        FString(TEXT("GE_VelrathInitialAttributes")));

    const UGameplayEffect* InitialEffect = EffectClass->GetDefaultObject<UGameplayEffect>();
    TestNotNull(TEXT("Initial Gameplay Effect class has a loadable CDO"), InitialEffect);
    if (InitialEffect == nullptr)
    {
        return false;
    }

    TestEqual(
        TEXT("Initial Gameplay Effect is Instant"),
        InitialEffect->DurationPolicy,
        EGameplayEffectDurationType::Instant);
    TestEqual(TEXT("Initial Gameplay Effect has exactly four modifiers"), InitialEffect->Modifiers.Num(), 4);

    struct FExpectedModifier
    {
        FGameplayAttribute Attribute;
        float Magnitude;
    };
    const TStaticArray<FExpectedModifier, 4> Expected = {
        FExpectedModifier{UVelrathAttributeSet::GetMaxHealthAttribute(), 1000.0f},
        FExpectedModifier{UVelrathAttributeSet::GetHealthAttribute(), 1000.0f},
        FExpectedModifier{UVelrathAttributeSet::GetMaxStaminaAttribute(), 100.0f},
        FExpectedModifier{UVelrathAttributeSet::GetStaminaAttribute(), 100.0f},
    };

    if (InitialEffect->Modifiers.Num() == Expected.Num())
    {
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
        {
            const FGameplayModifierInfo& Modifier = InitialEffect->Modifiers[Index];
            TestTrue(
                *FString::Printf(TEXT("Modifier %d targets the approved attribute in maxima-before-currents order"), Index),
                Modifier.Attribute == Expected[Index].Attribute);
            TestEqual(
                *FString::Printf(TEXT("Modifier %d uses Override"), Index),
                Modifier.ModifierOp,
                EGameplayModOp::Override);

            float Magnitude = 0.0f;
            TestTrue(
                *FString::Printf(TEXT("Modifier %d has a static magnitude"), Index),
                Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude));
            TestEqual(
                *FString::Printf(TEXT("Modifier %d matches the Combat Spec magnitude"), Index),
                Magnitude,
                Expected[Index].Magnitude);
        }
    }

    UVelrathAbilitySet* MissingEffectAsset = NewObject<UVelrathAbilitySet>(GetTransientPackage());
    TestFalse(
        TEXT("A startup asset with no initial effect fails closed"),
        MissingEffectAsset->ValidateInitialAttributesEffect(ValidationError));

    UGameplayEffect* WrongDuration = MakeValidInitialEffect();
    WrongDuration->DurationPolicy = EGameplayEffectDurationType::Infinite;
    TestFalse(
        TEXT("A non-Instant initial effect is rejected"),
        UVelrathAbilitySet::ValidateInitialAttributesEffect(WrongDuration, ValidationError));

    UGameplayEffect* MissingModifier = MakeValidInitialEffect();
    MissingModifier->Modifiers.Pop();
    TestFalse(
        TEXT("An initial effect with a missing modifier is rejected"),
        UVelrathAbilitySet::ValidateInitialAttributesEffect(MissingModifier, ValidationError));

    UGameplayEffect* WrongOperation = MakeValidInitialEffect();
    WrongOperation->Modifiers[0].ModifierOp = EGameplayModOp::Additive;
    TestFalse(
        TEXT("A non-Override initial modifier is rejected"),
        UVelrathAbilitySet::ValidateInitialAttributesEffect(WrongOperation, ValidationError));

    UGameplayEffect* WrongOrder = MakeValidInitialEffect();
    Swap(WrongOrder->Modifiers[0], WrongOrder->Modifiers[1]);
    TestFalse(
        TEXT("Current-before-maximum modifier order is rejected"),
        UVelrathAbilitySet::ValidateInitialAttributesEffect(WrongOrder, ValidationError));

    UGameplayEffect* WrongMagnitude = MakeValidInitialEffect();
    WrongMagnitude->Modifiers[0].ModifierMagnitude = FScalableFloat(-1.0f);
    TestFalse(
        TEXT("A negative initial magnitude is rejected"),
        UVelrathAbilitySet::ValidateInitialAttributesEffect(WrongMagnitude, ValidationError));

    UGameplayEffect* CurrentAboveMaximum = MakeValidInitialEffect();
    CurrentAboveMaximum->Modifiers[1].ModifierMagnitude = FScalableFloat(1001.0f);
    TestFalse(
        TEXT("An initial current value above its maximum is rejected"),
        UVelrathAbilitySet::ValidateInitialAttributesEffect(CurrentAboveMaximum, ValidationError));

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathAbilityGrantLifecycleTest,
    "VELRATH.GAS.Startup.AbilityGrantLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathAbilityGrantLifecycleTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull(TEXT("An Unreal test world is available"), World);
    if (World == nullptr)
    {
        return false;
    }

    UVelrathAbilitySet* ProductionAsset = LoadObject<UVelrathAbilitySet>(nullptr, StartupAssetPath);
    TestNotNull(TEXT("Lifecycle test loads the shared startup asset"), ProductionAsset);
    if (ProductionAsset == nullptr)
    {
        return false;
    }

    const auto SpawnConfiguredCharacter = [World](UVelrathAbilitySet& AbilitySet)
    {
        AVelrathCharacter* Character = World->SpawnActor<AVelrathCharacter>();
        if (Character != nullptr)
        {
            Character->SetStartupAbilitySet(&AbilitySet);
        }
        return Character;
    };

    const auto PossessCharacter = [World](AVelrathCharacter& Character)
    {
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (Controller != nullptr)
        {
            Controller->Possess(&Character);
        }
        return Controller;
    };

    UVelrathAbilitySet* EmptySet = MakeAbilitySet(*ProductionAsset, {});
    AVelrathCharacter* EmptyCharacter = SpawnConfiguredCharacter(*EmptySet);
    APlayerController* EmptyController = EmptyCharacter != nullptr ? PossessCharacter(*EmptyCharacter) : nullptr;
    TestNotNull(TEXT("Empty-set Character spawns"), EmptyCharacter);
    TestNotNull(TEXT("Empty-set Character is possessed"), EmptyController);
    if (EmptyCharacter != nullptr)
    {
        TestTrue(TEXT("An empty Ability Set completes once"), EmptyCharacter->AreStartupAbilitiesGranted());
        TestEqual(TEXT("An empty Ability Set stores no handles"), EmptyCharacter->GetStartupAbilityHandleCount(), 0);
    }

    UVelrathAbilitySet* SingleSet = MakeAbilitySet(
        *ProductionAsset,
        {UVelrathGrantTestAbilityOne::StaticClass()});
    AVelrathCharacter* Character = SpawnConfiguredCharacter(*SingleSet);
    APlayerController* Controller = Character != nullptr ? PossessCharacter(*Character) : nullptr;
    TestNotNull(TEXT("Single-grant Character spawns"), Character);
    TestNotNull(TEXT("Single-grant Character is possessed"), Controller);
    if (Character == nullptr || Controller == nullptr)
    {
        return false;
    }

    UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();
    TestNotNull(TEXT("Single-grant Character owns an ASC"), AbilitySystemComponent);
    if (AbilitySystemComponent == nullptr)
    {
        return false;
    }

    TestTrue(TEXT("Server completes the configured grant"), Character->AreStartupAbilitiesGranted());
    TestEqual(TEXT("Single grant stores one handle"), Character->GetStartupAbilityHandleCount(), 1);
    TestTrue(TEXT("Stored grant handle is valid"), Character->GetStartupAbilityHandles()[0].IsValid());
    TestEqual(TEXT("ASC contains one granted ability"), CountAbilitySpecs(*AbilitySystemComponent), 1);

    const FGameplayAbilitySpecHandle FirstHandle = Character->GetStartupAbilityHandles()[0];
    TestTrue(TEXT("Repeated grant request succeeds idempotently"), Character->GrantStartupAbilities());
    TestEqual(TEXT("Repeated grant request keeps one ability"), CountAbilitySpecs(*AbilitySystemComponent), 1);
    TestTrue(
        TEXT("Repeated grant request retains the original handle"),
        Character->GetStartupAbilityHandles()[0] == FirstHandle);

    Controller->UnPossess();
    Controller->Possess(Character);
    TestEqual(TEXT("Repossession does not duplicate abilities"), CountAbilitySpecs(*AbilitySystemComponent), 1);
    TestTrue(
        TEXT("Repossession retains the original handle"),
        Character->GetStartupAbilityHandles()[0] == FirstHandle);

    const FGameplayAbilitySpecHandle ExternalHandle = AbilitySystemComponent->GiveAbility(
        FGameplayAbilitySpec(UVelrathGrantTestAbilityTwo::StaticClass(), 1));
    TestTrue(TEXT("A separate system can grant its own ability"), ExternalHandle.IsValid());
    TestEqual(TEXT("ASC contains startup and external abilities"), CountAbilitySpecs(*AbilitySystemComponent), 2);
    TestTrue(TEXT("Owned startup grant can be removed"), Character->RemoveStartupAbilities());
    TestEqual(TEXT("Removal clears retained startup handles"), Character->GetStartupAbilityHandleCount(), 0);
    TestFalse(TEXT("Removal clears the startup grant-complete state"), Character->AreStartupAbilitiesGranted());
    TestEqual(TEXT("Removal leaves the other system's ability"), CountAbilitySpecs(*AbilitySystemComponent), 1);
    TestNotNull(
        TEXT("Other system's ability handle remains resolvable"),
        AbilitySystemComponent->FindAbilitySpecFromHandle(ExternalHandle));

    UVelrathAbilitySet* MultipleSet = MakeAbilitySet(
        *ProductionAsset,
        {UVelrathGrantTestAbilityOne::StaticClass(), UVelrathGrantTestAbilityTwo::StaticClass()});
    AVelrathCharacter* ReplacementCharacter = SpawnConfiguredCharacter(*MultipleSet);
    APlayerController* ReplacementController = ReplacementCharacter != nullptr
        ? PossessCharacter(*ReplacementCharacter)
        : nullptr;
    TestNotNull(TEXT("Replacement Character spawns"), ReplacementCharacter);
    TestNotNull(TEXT("Replacement Character is possessed"), ReplacementController);
    if (ReplacementCharacter != nullptr)
    {
        TestTrue(TEXT("Replacement Character receives its own grant sequence"), ReplacementCharacter->AreStartupAbilitiesGranted());
        TestEqual(TEXT("Multiple grants retain two handles"), ReplacementCharacter->GetStartupAbilityHandleCount(), 2);
        TestEqual(
            TEXT("Replacement ASC contains both configured abilities"),
            CountAbilitySpecs(*ReplacementCharacter->GetAbilitySystemComponent()),
            2);
        TestTrue(
            TEXT("Old and replacement Characters own different ASCs"),
            ReplacementCharacter->GetAbilitySystemComponent() != AbilitySystemComponent);
    }

    FText GrantError;
    FVelrathAbilitySetGrantedHandles ForeignHandles;
    TestTrue(
        TEXT("A direct grant records handles against the original ASC"),
        MultipleSet->GiveToAbilitySystem(*AbilitySystemComponent, ForeignHandles, GrantError));
    if (ReplacementCharacter != nullptr)
    {
        TestFalse(
            TEXT("Handles recorded for one ASC cannot be reused for another ASC"),
            MultipleSet->GiveToAbilitySystem(
                *ReplacementCharacter->GetAbilitySystemComponent(),
                ForeignHandles,
                GrantError));
    }
    TestFalse(
        TEXT("A different Ability Set cannot remove another set's retained handles"),
        SingleSet->TakeFromAbilitySystem(ForeignHandles, GrantError));
    TestTrue(
        TEXT("Directly owned grants can be removed from their original ASC"),
        MultipleSet->TakeFromAbilitySystem(ForeignHandles, GrantError));

    return true;
}

#endif
