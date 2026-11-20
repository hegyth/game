#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"
#include "Player/VelrathCharacter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace
{
FActiveGameplayEffectHandle ApplyInstantModifier(
    UAbilitySystemComponent& AbilitySystemComponent,
    const FGameplayAttribute& Attribute,
    const EGameplayModOp::Type ModifierOperation,
    const float Magnitude)
{
    UGameplayEffect* GameplayEffect = NewObject<UGameplayEffect>(GetTransientPackage());
    GameplayEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo& Modifier = GameplayEffect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = Attribute;
    Modifier.ModifierOp = ModifierOperation;
    Modifier.ModifierMagnitude = FScalableFloat(Magnitude);

    return AbilitySystemComponent.ApplyGameplayEffectToSelf(
        GameplayEffect,
        1.0f,
        AbilitySystemComponent.MakeEffectContext());
}

bool HasLifetimeReplicationEntry(
    const TArray<FLifetimeProperty>& LifetimeProperties,
    const FProperty& Property)
{
    return LifetimeProperties.ContainsByPredicate(
        [&Property](const FLifetimeProperty& LifetimeProperty)
        {
            return LifetimeProperty.RepIndex == Property.RepIndex;
        });
}

constexpr double AttributePieTimeoutSeconds = 15.0;

class FValidateVelrathAttributePieCommand final : public IAutomationLatentCommand
{
public:
    explicit FValidateVelrathAttributePieCommand(FAutomationTestBase* InTest)
        : Test(InTest)
        , StartTimeSeconds(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        if (GEngine == nullptr)
        {
            Test->AddError(TEXT("GEngine is unavailable while waiting for attribute PIE."));
            return true;
        }

        UWorld* PieWorld = nullptr;
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World() != nullptr)
            {
                PieWorld = WorldContext.World();
                break;
            }
        }

        APlayerController* PlayerController = PieWorld != nullptr ? PieWorld->GetFirstPlayerController() : nullptr;
        AVelrathCharacter* Character = PlayerController != nullptr
            ? Cast<AVelrathCharacter>(PlayerController->GetPawn())
            : nullptr;
        if (Character == nullptr)
        {
            if (FPlatformTime::Seconds() - StartTimeSeconds >= AttributePieTimeoutSeconds)
            {
                Test->AddError(TEXT("Timed out waiting for a possessed VELRATH Character in attribute PIE."));
                return true;
            }

            return false;
        }

        UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();
        const UVelrathAttributeSet* AttributeSet = Character->GetVelrathAttributeSet();
        Test->TestNotNull(TEXT("PIE Character owns its ASC"), AbilitySystemComponent);
        Test->TestNotNull(TEXT("PIE Character owns its UVelrathAttributeSet"), AttributeSet);
        if (AbilitySystemComponent == nullptr || AttributeSet == nullptr)
        {
            return true;
        }

        Test->TestTrue(TEXT("PIE Character is authoritative"), Character->HasAuthority());
        Test->TestTrue(TEXT("ASC Owner Actor remains the Character"), AbilitySystemComponent->GetOwnerActor() == Character);
        Test->TestTrue(TEXT("ASC Avatar Actor remains the Character"), AbilitySystemComponent->GetAvatarActor() == Character);
        Test->TestEqual(TEXT("PIE starts with Health 1000"), AttributeSet->GetHealth(), 1000.0f);
        Test->TestEqual(TEXT("PIE starts with MaxHealth 1000"), AttributeSet->GetMaxHealth(), 1000.0f);
        Test->TestEqual(TEXT("PIE starts with Stamina 100"), AttributeSet->GetStamina(), 100.0f);
        Test->TestEqual(TEXT("PIE starts with MaxStamina 100"), AttributeSet->GetMaxStamina(), 100.0f);

        int32 AttributeSetCount = 0;
        int32 TotalAttributeSetCount = 0;
        for (const UAttributeSet* SpawnedAttributeSet : AbilitySystemComponent->GetSpawnedAttributes())
        {
            if (SpawnedAttributeSet != nullptr)
            {
                ++TotalAttributeSetCount;
                if (SpawnedAttributeSet->IsA<UVelrathAttributeSet>())
                {
                    ++AttributeSetCount;
                }
            }
        }
        Test->TestEqual(TEXT("PIE ASC contains exactly one UVelrathAttributeSet"), AttributeSetCount, 1);
        Test->TestEqual(TEXT("PIE ASC contains no duplicate AttributeSets"), TotalAttributeSetCount, 1);

        ApplyInstantModifier(
            *AbilitySystemComponent,
            UVelrathAttributeSet::GetHealthAttribute(),
            EGameplayModOp::Additive,
            -100.0f);
        Test->TestEqual(TEXT("Authority Gameplay Effect changes PIE Health"), AttributeSet->GetHealth(), 900.0f);
        ApplyInstantModifier(
            *AbilitySystemComponent,
            UVelrathAttributeSet::GetStaminaAttribute(),
            EGameplayModOp::Additive,
            -25.0f);
        Test->TestEqual(TEXT("Authority Gameplay Effect changes PIE Stamina"), AttributeSet->GetStamina(), 75.0f);

        PlayerController->UnPossess();
        PlayerController->Possess(Character);

        Test->TestTrue(TEXT("PlayerController repossesses the same Character"), PlayerController->GetPawn() == Character);
        Test->TestEqual(
            TEXT("Repeated possession does not reinitialize Health"),
            Character->GetVelrathAttributeSet()->GetHealth(),
            900.0f);
        Test->TestEqual(
            TEXT("Repeated possession does not reinitialize Stamina"),
            Character->GetVelrathAttributeSet()->GetStamina(),
            75.0f);
        Test->TestTrue(
            TEXT("Repeated possession preserves ASC Owner Actor"),
            AbilitySystemComponent->GetOwnerActor() == Character);
        Test->TestTrue(
            TEXT("Repeated possession preserves ASC Avatar Actor"),
            AbilitySystemComponent->GetAvatarActor() == Character);

        AttributeSetCount = 0;
        TotalAttributeSetCount = 0;
        for (const UAttributeSet* SpawnedAttributeSet : AbilitySystemComponent->GetSpawnedAttributes())
        {
            if (SpawnedAttributeSet != nullptr)
            {
                ++TotalAttributeSetCount;
                if (SpawnedAttributeSet->IsA<UVelrathAttributeSet>())
                {
                    ++AttributeSetCount;
                }
            }
        }
        Test->TestEqual(
            TEXT("Repeated possession does not duplicate UVelrathAttributeSet"),
            AttributeSetCount,
            1);
        Test->TestEqual(
            TEXT("Repeated possession preserves exactly one total AttributeSet"),
            TotalAttributeSetCount,
            1);

        return true;
    }

private:
    FAutomationTestBase* Test;
    double StartTimeSeconds;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathHealthAttributeSetTest,
    "VELRATH.GAS.Attributes.Health",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathHealthAttributeSetTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull(TEXT("An Unreal test world is available"), World);
    if (World == nullptr)
    {
        return false;
    }

    AVelrathCharacter* Character = World->SpawnActor<AVelrathCharacter>();
    TestNotNull(TEXT("A VELRATH Character can be spawned in the test world"), Character);
    if (Character == nullptr)
    {
        return false;
    }

    UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();
    const UVelrathAttributeSet* AttributeSet = Character->GetVelrathAttributeSet();
    TestNotNull(TEXT("Character owns its AbilitySystemComponent"), AbilitySystemComponent);
    TestNotNull(TEXT("Character owns a UVelrathAttributeSet"), AttributeSet);
    if (AbilitySystemComponent == nullptr || AttributeSet == nullptr)
    {
        return false;
    }

    AbilitySystemComponent->InitAbilityActorInfo(Character, Character);
    AbilitySystemComponent->AddAttributeSetSubobject(const_cast<UVelrathAttributeSet*>(AttributeSet));

    TestEqual(TEXT("Initial Health is 1000"), AttributeSet->GetHealth(), 1000.0f);
    TestEqual(TEXT("Initial MaxHealth is 1000"), AttributeSet->GetMaxHealth(), 1000.0f);
    TestEqual(TEXT("Initial Stamina is 100"), AttributeSet->GetStamina(), 100.0f);
    TestEqual(TEXT("Initial MaxStamina is 100"), AttributeSet->GetMaxStamina(), 100.0f);

    int32 VelrathAttributeSetCount = 0;
    for (const UAttributeSet* SpawnedAttributeSet : AbilitySystemComponent->GetSpawnedAttributes())
    {
        if (SpawnedAttributeSet != nullptr && SpawnedAttributeSet->IsA<UVelrathAttributeSet>())
        {
            ++VelrathAttributeSetCount;
        }
    }
    TestEqual(TEXT("ASC contains exactly one UVelrathAttributeSet"), VelrathAttributeSetCount, 1);
    TestTrue(
        TEXT("ASC resolves Health to the Character-owned AttributeSet"),
        AbilitySystemComponent->HasAttributeSetForAttribute(UVelrathAttributeSet::GetHealthAttribute()));
    TestTrue(
        TEXT("ASC resolves Stamina to the Character-owned AttributeSet"),
        AbilitySystemComponent->HasAttributeSetForAttribute(UVelrathAttributeSet::GetStaminaAttribute()));

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetHealthAttribute(),
        EGameplayModOp::Override,
        -100.0f);
    TestEqual(TEXT("Gameplay Effect cannot lower Health below zero"), AttributeSet->GetHealth(), 0.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetHealthAttribute(),
        EGameplayModOp::Override,
        1500.0f);
    TestEqual(TEXT("Gameplay Effect cannot raise Health above MaxHealth"), AttributeSet->GetHealth(), 1000.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetHealthAttribute(),
        EGameplayModOp::Override,
        900.0f);
    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetMaxHealthAttribute(),
        EGameplayModOp::Override,
        600.0f);
    TestEqual(TEXT("Gameplay Effect changes MaxHealth"), AttributeSet->GetMaxHealth(), 600.0f);
    TestEqual(TEXT("Reducing MaxHealth clamps existing Health"), AttributeSet->GetHealth(), 600.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetHealthAttribute(),
        EGameplayModOp::Additive,
        -125.0f);
    TestEqual(TEXT("Gameplay Effect modifies Health through GAS"), AttributeSet->GetHealth(), 475.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetStaminaAttribute(),
        EGameplayModOp::Override,
        -10.0f);
    TestEqual(TEXT("Gameplay Effect cannot lower Stamina below zero"), AttributeSet->GetStamina(), 0.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetStaminaAttribute(),
        EGameplayModOp::Override,
        150.0f);
    TestEqual(TEXT("Gameplay Effect cannot raise Stamina above MaxStamina"), AttributeSet->GetStamina(), 100.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetStaminaAttribute(),
        EGameplayModOp::Override,
        80.0f);
    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetMaxStaminaAttribute(),
        EGameplayModOp::Override,
        60.0f);
    TestEqual(TEXT("Gameplay Effect changes MaxStamina"), AttributeSet->GetMaxStamina(), 60.0f);
    TestEqual(TEXT("Reducing MaxStamina clamps existing Stamina"), AttributeSet->GetStamina(), 60.0f);

    ApplyInstantModifier(
        *AbilitySystemComponent,
        UVelrathAttributeSet::GetStaminaAttribute(),
        EGameplayModOp::Additive,
        -15.0f);
    TestEqual(TEXT("Gameplay Effect modifies Stamina through GAS"), AttributeSet->GetStamina(), 45.0f);

    FProperty* HealthProperty = FindFProperty<FProperty>(UVelrathAttributeSet::StaticClass(), TEXT("Health"));
    FProperty* MaxHealthProperty = FindFProperty<FProperty>(UVelrathAttributeSet::StaticClass(), TEXT("MaxHealth"));
    FProperty* StaminaProperty = FindFProperty<FProperty>(UVelrathAttributeSet::StaticClass(), TEXT("Stamina"));
    FProperty* MaxStaminaProperty = FindFProperty<FProperty>(UVelrathAttributeSet::StaticClass(), TEXT("MaxStamina"));
    TestNotNull(TEXT("Health reflected property exists"), HealthProperty);
    TestNotNull(TEXT("MaxHealth reflected property exists"), MaxHealthProperty);
    TestNotNull(TEXT("Stamina reflected property exists"), StaminaProperty);
    TestNotNull(TEXT("MaxStamina reflected property exists"), MaxStaminaProperty);

    TArray<FLifetimeProperty> LifetimeProperties;
    AttributeSet->GetLifetimeReplicatedProps(LifetimeProperties);

    if (HealthProperty != nullptr)
    {
        TestTrue(TEXT("Health is replicated"), HealthProperty->HasAnyPropertyFlags(CPF_Net));
        TestTrue(TEXT("Health uses RepNotify"), HealthProperty->HasAnyPropertyFlags(CPF_RepNotify));
        TestEqual(TEXT("Health uses OnRep_Health"), HealthProperty->RepNotifyFunc, FName(TEXT("OnRep_Health")));
        TestTrue(
            TEXT("Health is registered by GetLifetimeReplicatedProps"),
            HasLifetimeReplicationEntry(LifetimeProperties, *HealthProperty));
    }

    if (MaxHealthProperty != nullptr)
    {
        TestTrue(TEXT("MaxHealth is replicated"), MaxHealthProperty->HasAnyPropertyFlags(CPF_Net));
        TestTrue(TEXT("MaxHealth uses RepNotify"), MaxHealthProperty->HasAnyPropertyFlags(CPF_RepNotify));
        TestEqual(TEXT("MaxHealth uses OnRep_MaxHealth"), MaxHealthProperty->RepNotifyFunc, FName(TEXT("OnRep_MaxHealth")));
        TestTrue(
            TEXT("MaxHealth is registered by GetLifetimeReplicatedProps"),
            HasLifetimeReplicationEntry(LifetimeProperties, *MaxHealthProperty));
    }

    if (StaminaProperty != nullptr)
    {
        TestTrue(TEXT("Stamina is replicated"), StaminaProperty->HasAnyPropertyFlags(CPF_Net));
        TestTrue(TEXT("Stamina uses RepNotify"), StaminaProperty->HasAnyPropertyFlags(CPF_RepNotify));
        TestEqual(TEXT("Stamina uses OnRep_Stamina"), StaminaProperty->RepNotifyFunc, FName(TEXT("OnRep_Stamina")));
        TestTrue(
            TEXT("Stamina is registered by GetLifetimeReplicatedProps"),
            HasLifetimeReplicationEntry(LifetimeProperties, *StaminaProperty));
    }

    if (MaxStaminaProperty != nullptr)
    {
        TestTrue(TEXT("MaxStamina is replicated"), MaxStaminaProperty->HasAnyPropertyFlags(CPF_Net));
        TestTrue(TEXT("MaxStamina uses RepNotify"), MaxStaminaProperty->HasAnyPropertyFlags(CPF_RepNotify));
        TestEqual(TEXT("MaxStamina uses OnRep_MaxStamina"), MaxStaminaProperty->RepNotifyFunc, FName(TEXT("OnRep_MaxStamina")));
        TestTrue(
            TEXT("MaxStamina is registered by GetLifetimeReplicatedProps"),
            HasLifetimeReplicationEntry(LifetimeProperties, *MaxStaminaProperty));
    }

    UFunction* HealthRepNotify = UVelrathAttributeSet::StaticClass()->FindFunctionByName(TEXT("OnRep_Health"));
    UFunction* MaxHealthRepNotify = UVelrathAttributeSet::StaticClass()->FindFunctionByName(TEXT("OnRep_MaxHealth"));
    UFunction* StaminaRepNotify = UVelrathAttributeSet::StaticClass()->FindFunctionByName(TEXT("OnRep_Stamina"));
    UFunction* MaxStaminaRepNotify = UVelrathAttributeSet::StaticClass()->FindFunctionByName(TEXT("OnRep_MaxStamina"));
    TestNotNull(TEXT("OnRep_Health callback exists"), HealthRepNotify);
    TestNotNull(TEXT("OnRep_MaxHealth callback exists"), MaxHealthRepNotify);
    TestNotNull(TEXT("OnRep_Stamina callback exists"), StaminaRepNotify);
    TestNotNull(TEXT("OnRep_MaxStamina callback exists"), MaxStaminaRepNotify);
    TestEqual(TEXT("OnRep_Health receives the old value"), HealthRepNotify != nullptr ? HealthRepNotify->NumParms : 0, 1);
    TestEqual(TEXT("OnRep_MaxHealth receives the old value"), MaxHealthRepNotify != nullptr ? MaxHealthRepNotify->NumParms : 0, 1);
    TestEqual(TEXT("OnRep_Stamina receives the old value"), StaminaRepNotify != nullptr ? StaminaRepNotify->NumParms : 0, 1);
    TestEqual(TEXT("OnRep_MaxStamina receives the old value"), MaxStaminaRepNotify != nullptr ? MaxStaminaRepNotify->NumParms : 0, 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathHealthAttributePieTest,
    "VELRATH.GAS.Attributes.PIE.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathHealthAttributePieTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/L_Dev")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FValidateVelrathAttributePieCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

    return true;
}

#endif
