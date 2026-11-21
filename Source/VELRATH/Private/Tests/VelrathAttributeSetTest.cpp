#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/VelrathAbilitySet.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HAL/PlatformTime.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
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

        if (bBlueprintReinstanceRequested && Character == nullptr)
        {
            Character = OriginalCharacter.Get();
            if (Character == nullptr)
            {
                UBlueprint* CharacterBlueprint = LoadObject<UBlueprint>(
                    nullptr,
                    TEXT("/Game/Characters/BP_VelrathCharacter.BP_VelrathCharacter"));
                const AVelrathCharacter* CharacterDefaults =
                    CharacterBlueprint != nullptr && CharacterBlueprint->GeneratedClass != nullptr
                    ? Cast<AVelrathCharacter>(CharacterBlueprint->GeneratedClass->GetDefaultObject())
                    : nullptr;
                Test->TestNotNull(
                    TEXT("Reinstanced BP_VelrathCharacter retains loadable defaults"),
                    CharacterDefaults);
                if (CharacterDefaults != nullptr)
                {
                    Test->TestNotNull(
                        TEXT("Reinstanced BP_VelrathCharacter retains the startup asset reference"),
                        CharacterDefaults->GetStartupAbilitySet());
                }
                Test->AddInfo(TEXT("Editor Blueprint compilation destroyed the live PIE Actor and left no replacement pawn; recorded as editor-only reinstance, not gameplay respawn."));
                return true;
            }
        }

        if (Character == nullptr)
        {
            if (FPlatformTime::Seconds() - StartTimeSeconds >= AttributePieTimeoutSeconds)
            {
                Test->AddError(TEXT("Timed out waiting for a possessed VELRATH Character in attribute PIE."));
                return true;
            }

            return false;
        }

        if (bBlueprintReinstanceRequested)
        {
            UAbilitySystemComponent* ReinstancedAbilitySystemComponent = Character->GetAbilitySystemComponent();
            const UVelrathAttributeSet* ReinstancedAttributeSet = Character->GetVelrathAttributeSet();
            Test->TestNotNull(TEXT("Reinstanced PIE Character retains its ASC"), ReinstancedAbilitySystemComponent);
            Test->TestNotNull(TEXT("Reinstanced PIE Character retains its AttributeSet"), ReinstancedAttributeSet);
            if (ReinstancedAbilitySystemComponent == nullptr || ReinstancedAttributeSet == nullptr)
            {
                return true;
            }

            Test->TestTrue(
                TEXT("Blueprint reinstance preserves the completed initialization guard"),
                Character->AreDefaultAttributesInitialized());
            Test->TestNotNull(
                TEXT("Blueprint reinstance preserves the startup asset reference"),
                Character->GetStartupAbilitySet());
            Test->TestEqual(
                TEXT("Blueprint reinstance does not reset changed Health"),
                ReinstancedAttributeSet->GetHealth(),
                900.0f);
            Test->TestEqual(
                TEXT("Blueprint reinstance does not reset changed Stamina"),
                ReinstancedAttributeSet->GetStamina(),
                75.0f);

            int32 ReinstancedAttributeSetCount = 0;
            int32 ReinstancedTotalAttributeSetCount = 0;
            for (const UAttributeSet* SpawnedAttributeSet : ReinstancedAbilitySystemComponent->GetSpawnedAttributes())
            {
                if (SpawnedAttributeSet != nullptr)
                {
                    ++ReinstancedTotalAttributeSetCount;
                    if (SpawnedAttributeSet->IsA<UVelrathAttributeSet>())
                    {
                        ++ReinstancedAttributeSetCount;
                    }
                }
            }
            Test->TestEqual(
                TEXT("Blueprint reinstance retains exactly one UVelrathAttributeSet"),
                ReinstancedAttributeSetCount,
                1);
            Test->TestEqual(
                TEXT("Blueprint reinstance retains exactly one total AttributeSet"),
                ReinstancedTotalAttributeSetCount,
                1);

            if (Character != OriginalCharacter.Get())
            {
                Test->AddInfo(TEXT("Editor Blueprint compilation replaced the live PIE Actor; this was validated as editor reinstance, not gameplay respawn."));
            }

            return true;
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
        Test->TestNotNull(TEXT("PIE Character has the shared startup asset"), Character->GetStartupAbilitySet());
        Test->TestTrue(TEXT("PIE server completed startup attribute initialization"), Character->AreDefaultAttributesInitialized());
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

        UBlueprint* CharacterBlueprint = LoadObject<UBlueprint>(
            nullptr,
            TEXT("/Game/Characters/BP_VelrathCharacter.BP_VelrathCharacter"));
        Test->TestNotNull(TEXT("Lifecycle test loads BP_VelrathCharacter for reinstance validation"), CharacterBlueprint);
        if (CharacterBlueprint == nullptr)
        {
            return true;
        }

        OriginalCharacter = Character;
        bBlueprintReinstanceRequested = true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CharacterBlueprint);
        FKismetEditorUtilities::CompileBlueprint(CharacterBlueprint);
        return false;
    }

private:
    FAutomationTestBase* Test;
    double StartTimeSeconds;
    TWeakObjectPtr<AVelrathCharacter> OriginalCharacter;
    bool bBlueprintReinstanceRequested = false;
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

    TestEqual(TEXT("Health is structurally zero before the initialization effect"), AttributeSet->GetHealth(), 0.0f);
    TestEqual(TEXT("MaxHealth is structurally zero before the initialization effect"), AttributeSet->GetMaxHealth(), 0.0f);
    TestEqual(TEXT("Stamina is structurally zero before the initialization effect"), AttributeSet->GetStamina(), 0.0f);
    TestEqual(TEXT("MaxStamina is structurally zero before the initialization effect"), AttributeSet->GetMaxStamina(), 0.0f);

    UVelrathAbilitySet* StartupAsset = LoadObject<UVelrathAbilitySet>(
        nullptr,
        TEXT("/Game/Abilities/DA_VelrathSharedStartup.DA_VelrathSharedStartup"));
    TestNotNull(TEXT("Unit test loads the shared startup asset"), StartupAsset);
    if (StartupAsset == nullptr)
    {
        return false;
    }

    const FGameplayEffectSpecHandle StartupSpec = AbilitySystemComponent->MakeOutgoingSpec(
        StartupAsset->GetInitialAttributesEffect(),
        1.0f,
        AbilitySystemComponent->MakeEffectContext());
    TestTrue(TEXT("GAS creates a spec for the authored initialization effect"), StartupSpec.IsValid());
    if (!StartupSpec.IsValid())
    {
        return false;
    }

    const FActiveGameplayEffectHandle StartupHandle =
        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*StartupSpec.Data.Get());
    TestTrue(TEXT("GAS applies the authored initialization effect"), StartupHandle.WasSuccessfullyApplied());
    TestEqual(TEXT("Initialization effect sets Health to 1000"), AttributeSet->GetHealth(), 1000.0f);
    TestEqual(TEXT("Initialization effect sets MaxHealth to 1000"), AttributeSet->GetMaxHealth(), 1000.0f);
    TestEqual(TEXT("Initialization effect sets Stamina to 100"), AttributeSet->GetStamina(), 100.0f);
    TestEqual(TEXT("Initialization effect sets MaxStamina to 100"), AttributeSet->GetMaxStamina(), 100.0f);

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

    AVelrathCharacter* MissingConfigCharacter = World->SpawnActor<AVelrathCharacter>();
    APlayerController* MissingConfigController = World->SpawnActor<APlayerController>();
    TestNotNull(TEXT("Missing-config Character can be spawned"), MissingConfigCharacter);
    TestNotNull(TEXT("Missing-config controller can be spawned"), MissingConfigController);
    if (MissingConfigCharacter != nullptr && MissingConfigController != nullptr)
    {
        AddExpectedError(
            TEXT("StartupAbilitySet is not configured"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        MissingConfigController->Possess(MissingConfigCharacter);

        TestFalse(
            TEXT("Missing startup configuration leaves initialization incomplete"),
            MissingConfigCharacter->AreDefaultAttributesInitialized());
        TestEqual(
            TEXT("Missing startup configuration leaves Health at structural zero"),
            MissingConfigCharacter->GetVelrathAttributeSet()->GetHealth(),
            0.0f);
        TestEqual(
            TEXT("Missing startup configuration leaves MaxHealth at structural zero"),
            MissingConfigCharacter->GetVelrathAttributeSet()->GetMaxHealth(),
            0.0f);
        TestEqual(
            TEXT("Missing startup configuration leaves Stamina at structural zero"),
            MissingConfigCharacter->GetVelrathAttributeSet()->GetStamina(),
            0.0f);
        TestEqual(
            TEXT("Missing startup configuration leaves MaxStamina at structural zero"),
            MissingConfigCharacter->GetVelrathAttributeSet()->GetMaxStamina(),
            0.0f);
    }

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
