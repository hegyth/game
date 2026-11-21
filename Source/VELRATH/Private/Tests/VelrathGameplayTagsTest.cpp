#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/VELRATHGameplayTags.h"
#include "GameplayTagsManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathNativeGameplayTagsTest,
    "VELRATH.GAS.Tags.NativeRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathNativeGameplayTagsTest::RunTest(const FString& Parameters)
{
    using namespace VelrathGameplayTags;

    const TArray<FGameplayTag> Tags = {
        Input_Move.GetTag(),
        Input_Look.GetTag(),
        Input_Ability_Attack_Light.GetTag(),
        Input_Ability_Attack_Heavy.GetTag(),
        Input_Ability_Block.GetTag(),
        Input_Ability_Parry.GetTag(),
        Input_Ability_Dodge.GetTag(),
        Input_Ability_Echo_Cast.GetTag(),
        Input_Ability_Echo_Disrupt.GetTag(),
        State_Combat_Locomotion.GetTag(),
        State_Combat_LightAttack.GetTag(),
        State_Combat_HeavyAttack.GetTag(),
        State_Combat_Block.GetTag(),
        State_Combat_Parry.GetTag(),
        State_Combat_Dodge.GetTag(),
        State_Combat_AbilityCast.GetTag(),
        State_Combat_EchoCast.GetTag(),
        State_Control_Stagger.GetTag(),
        State_Control_GuardBreak.GetTag(),
        State_Life_Dead.GetTag(),
        State_Life_Respawn.GetTag(),
        State_Echo_Forming.GetTag(),
        State_Echo_Active.GetTag(),
        State_Echo_Expired.GetTag(),
        State_Echo_Destroyed.GetTag(),
        Ability_Attack_Light.GetTag(),
        Ability_Attack_Heavy.GetTag(),
        Ability_Block.GetTag(),
        Ability_Parry.GetTag(),
        Ability_Dodge.GetTag(),
        Ability_Echo_Cast.GetTag(),
        Ability_Echo_Disrupt.GetTag(),
        Cooldown_Parry.GetTag(),
        Cooldown_Dodge.GetTag(),
        Cooldown_Echo.GetTag(),
        Effect_Damage.GetTag(),
        Effect_StaminaCost.GetTag(),
        Effect_StaminaRegen.GetTag(),
        Effect_Slow.GetTag(),
        Effect_Stagger.GetTag(),
        Event_Attack_Hit.GetTag(),
        Event_Attack_Parried.GetTag(),
        Event_Echo_Spawned.GetTag(),
        Event_Echo_Expired.GetTag(),
    };

    TSet<FGameplayTag> UniqueTags;
    const UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
    for (const FGameplayTag& Tag : Tags)
    {
        TestTrue(TEXT("Every native tag is valid"), Tag.IsValid());
        TestEqual(
            *FString::Printf(TEXT("Tag manager resolves %s"), *Tag.ToString()),
            TagManager.RequestGameplayTag(Tag.GetTagName(), false),
            Tag);
        UniqueTags.Add(Tag);
    }

    TestEqual(TEXT("The registry contains exactly 44 canonical tags"), Tags.Num(), 44);
    TestEqual(TEXT("Every canonical tag has one unique path"), UniqueTags.Num(), Tags.Num());

    TestFalse(
        TEXT("Deprecated State.Movement alias is not registered"),
        TagManager.RequestGameplayTag(FName(TEXT("State.Movement")), false).IsValid());
    TestFalse(
        TEXT("Deprecated State.Attack.Light alias is not registered"),
        TagManager.RequestGameplayTag(FName(TEXT("State.Attack.Light")), false).IsValid());
    TestFalse(
        TEXT("Deprecated State.Attack.Heavy alias is not registered"),
        TagManager.RequestGameplayTag(FName(TEXT("State.Attack.Heavy")), false).IsValid());

    return true;
}

#endif
