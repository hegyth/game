#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/VelrathAbilitySet.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"
#include "Game/VelrathGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Player/VelrathCharacter.h"
#include "Tests/VelrathTestGameplayAbilities.h"

#if ENABLE_PIE_NETWORK_TEST

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogVelrathGasNetworkTest, Log, All);

constexpr int32 OwningClientIndex = 0;
constexpr int32 RemoteClientIndex = 1;
constexpr float InitialHealth = 1000.0f;
constexpr float ReplicatedHealth = 900.0f;
constexpr float ClientOnlyHealth = 111.0f;
constexpr float CorrectedHealth = 850.0f;
constexpr float ReplicatedMaxHealth = 1200.0f;
constexpr float InitialStamina = 100.0f;
constexpr float ReplicatedStamina = 75.0f;
constexpr float ClientOnlyStamina = 11.0f;
constexpr float CorrectedStamina = 60.0f;
constexpr float ReplicatedMaxStamina = 80.0f;

struct FVelrathGasNetworkState : public FBasePIENetworkComponentState
{
    AVelrathCharacter* TargetCharacter = nullptr;
    AVelrathCharacter* PreviousTargetCharacter = nullptr;
    const UVelrathAttributeSet* PreviousAttributeSet = nullptr;
    TWeakObjectPtr<UAbilitySystemComponent> PreviousAbilitySystemComponent;
};

AVelrathCharacter* FindCharacterForPlayerId(UWorld& World, const int32 PlayerId)
{
    for (TActorIterator<AVelrathCharacter> It(&World); It; ++It)
    {
        AVelrathCharacter* Character = *It;
        const APlayerState* PlayerState = Character->GetPlayerState();
        if (IsValid(Character) && PlayerState != nullptr && PlayerState->GetPlayerId() == PlayerId)
        {
            return Character;
        }
    }

    return nullptr;
}

bool HasReadyGasState(const AVelrathCharacter* Character)
{
    if (!IsValid(Character))
    {
        return false;
    }

    const UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();
    const UVelrathAttributeSet* AttributeSet = Character->GetVelrathAttributeSet();
    return AbilitySystemComponent != nullptr
        && AttributeSet != nullptr
        && AbilitySystemComponent->AbilityActorInfo.IsValid()
        && AbilitySystemComponent->GetOwnerActor() == Character
        && AbilitySystemComponent->GetAvatarActor() == Character;
}

void ApplyInstantOverride(
    UAbilitySystemComponent& AbilitySystemComponent,
    const FGameplayAttribute& Attribute,
    const float Magnitude)
{
    UGameplayEffect* GameplayEffect = NewObject<UGameplayEffect>(GetTransientPackage());
    GameplayEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo& Modifier = GameplayEffect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = Attribute;
    Modifier.ModifierOp = EGameplayModOp::Override;
    Modifier.ModifierMagnitude = FScalableFloat(Magnitude);

    AbilitySystemComponent.ApplyGameplayEffectToSelf(
        GameplayEffect,
        1.0f,
        AbilitySystemComponent.MakeEffectContext());
}

float GetHealth(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetVelrathAttributeSet()->GetHealth();
}

float GetMaxHealth(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetVelrathAttributeSet()->GetMaxHealth();
}

float GetStamina(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetVelrathAttributeSet()->GetStamina();
}

float GetMaxStamina(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetVelrathAttributeSet()->GetMaxStamina();
}

int32 GetAbilitySpecCount(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetAbilitySystemComponent()->GetActivatableAbilities().Num();
}

bool HasTestAbility(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetAbilitySystemComponent()->FindAbilitySpecFromClass(
        UVelrathGrantTestAbilityOne::StaticClass()) != nullptr;
}

UVelrathAbilitySet* MakeNetworkTestAbilitySet(const AVelrathCharacter& Character)
{
    const UVelrathAbilitySet* SourceSet = Character.GetStartupAbilitySet();
    if (SourceSet == nullptr)
    {
        return nullptr;
    }

    UVelrathAbilitySet* TestSet = NewObject<UVelrathAbilitySet>(GetTransientPackage());
    TestSet->SetInitialAttributesEffect(SourceSet->GetInitialAttributesEffect());
    TestSet->SetGrantedAbilities({UVelrathGrantTestAbilityOne::StaticClass()});
    return TestSet;
}
}

NETWORK_TEST_CLASS(FVelrathGasReplicationNetworkTest, "VELRATH.GAS.Network.ReplicationLifecycle")
{
    FPIENetworkComponent<FVelrathGasNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
    int32 TargetPlayerId = INDEX_NONE;

    BEFORE_EACH()
    {
        FNetworkComponentBuilder<FVelrathGasNetworkState>()
            .WithClients(2)
            .AsListenServer()
            .WithGameInstanceClass(UGameInstance::StaticClass())
            .WithGameMode(AVelrathGameMode::StaticClass())
            .Build(Network);
    }

    TEST_METHOD(ServerOwnerAndRemote_ReplicateAttributesAndRespawnCleanly)
    {
        Network
            .UntilServer(TEXT("Wait for two connected client pawns"), [this](FVelrathGasNetworkState& State)
            {
                if (State.ClientConnections.Num() != 2 || State.ClientConnections[OwningClientIndex] == nullptr)
                {
                    return false;
                }

                APlayerController* TargetController = State.ClientConnections[OwningClientIndex]->PlayerController;
                AVelrathCharacter* Character = TargetController != nullptr
                    ? Cast<AVelrathCharacter>(TargetController->GetPawn())
                    : nullptr;
                if (!HasReadyGasState(Character) || Character->GetPlayerState() == nullptr)
                {
                    return false;
                }

                State.TargetCharacter = Character;
                TargetPlayerId = Character->GetPlayerState()->GetPlayerId();
                return TargetPlayerId != INDEX_NONE;
            })
            .UntilClients(TEXT("Resolve the same pawn in both client worlds"), [this](FVelrathGasNetworkState& State)
            {
                State.TargetCharacter = FindCharacterForPlayerId(*State.World, TargetPlayerId);
                return HasReadyGasState(State.TargetCharacter)
                    && GetHealth(State) == InitialHealth
                    && GetMaxHealth(State) == InitialHealth
                    && GetStamina(State) == InitialStamina
                    && GetMaxStamina(State) == InitialStamina;
            })
            .ThenServer(TEXT("Validate authoritative initial GAS state"), [this](FVelrathGasNetworkState& State)
            {
                AVelrathCharacter* Character = State.TargetCharacter;
                APlayerController* Controller = Cast<APlayerController>(Character->GetController());
                UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();

                ASSERT_THAT(IsTrue(Character->HasAuthority(), TEXT("Target pawn must be authoritative in the server world.")));
                ASSERT_THAT(IsNotNull(Controller, TEXT("Server PlayerController must possess the target pawn.")));
                ASSERT_THAT(IsTrue(Controller->GetPawn() == Character, TEXT("Server PlayerController ownership must resolve to the target pawn.")));
                ASSERT_THAT(AreEqual(EGameplayEffectReplicationMode::Mixed, AbilitySystemComponent->ReplicationMode, TEXT("Character ASC must use Mixed replication mode.")));
                ASSERT_THAT(IsTrue(Character->AreDefaultAttributesInitialized(), TEXT("Server must complete the initialization effect after ActorInfo.")));
                ASSERT_THAT(IsTrue(Character->AreStartupAbilitiesGranted(), TEXT("Server must complete the production startup Ability Set, including an empty set.")));
                ASSERT_THAT(AreEqual(0, Character->GetStartupAbilityHandleCount(), TEXT("Production startup data must not contain placeholder combat abilities.")));
                ASSERT_THAT(IsNear(InitialHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("Server Health must initialize to 1000.")));
                ASSERT_THAT(IsNear(InitialHealth, GetMaxHealth(State), UE_SMALL_NUMBER, TEXT("Server MaxHealth must initialize to 1000.")));
                ASSERT_THAT(IsNear(InitialStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("Server Stamina must initialize to 100.")));
                ASSERT_THAT(IsNear(InitialStamina, GetMaxStamina(State), UE_SMALL_NUMBER, TEXT("Server MaxStamina must initialize to 100.")));

                UE_LOG(LogVelrathGasNetworkTest, Display,
                    TEXT("GAS-006 initial state: serverNetMode=%d clients=%d targetPlayerId=%d Health=%.0f MaxHealth=%.0f Stamina=%.0f MaxStamina=%.0f"),
                    static_cast<int32>(State.World->GetNetMode()), State.ClientCount, TargetPlayerId,
                    GetHealth(State), GetMaxHealth(State), GetStamina(State), GetMaxStamina(State));
            })
            .ThenServer(TEXT("Grant a test startup ability through the Ability Set lifecycle"), [this](FVelrathGasNetworkState& State)
            {
                AVelrathCharacter* Character = State.TargetCharacter;
                UVelrathAbilitySet* TestSet = MakeNetworkTestAbilitySet(*Character);
                ASSERT_THAT(IsNotNull(TestSet, TEXT("Network test must create a valid transient Ability Set.")));
                ASSERT_THAT(IsTrue(Character->RemoveStartupAbilities(), TEXT("Server must clear the completed empty production grant state.")));
                Character->SetStartupAbilitySet(TestSet);
                ASSERT_THAT(IsTrue(Character->GrantStartupAbilities(), TEXT("Server must grant the configured test ability.")));
                ASSERT_THAT(IsTrue(Character->AreStartupAbilitiesGranted(), TEXT("Server must retain the test grant state.")));
                ASSERT_THAT(AreEqual(1, Character->GetStartupAbilityHandleCount(), TEXT("Server must retain one valid grant handle.")));
                ASSERT_THAT(IsTrue(Character->GetStartupAbilityHandles()[0].IsValid(), TEXT("Retained server ability handle must be valid.")));
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("Server ASC must contain one test ability spec.")));

                const FGameplayAbilitySpecHandle FirstGrantHandle = Character->GetStartupAbilityHandles()[0];
                ASSERT_THAT(IsTrue(Character->GrantStartupAbilities(), TEXT("A duplicate server grant request must be idempotent.")));
                ASSERT_THAT(AreEqual(1, Character->GetStartupAbilityHandleCount(), TEXT("A duplicate server grant request must retain exactly one handle.")));
                ASSERT_THAT(IsTrue(Character->GetStartupAbilityHandles()[0] == FirstGrantHandle, TEXT("A duplicate server grant request must retain the original handle.")));
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("A duplicate server grant request must not add an ability spec.")));
                Character->ForceNetUpdate();
            })
            .UntilClient(TEXT("Wait for owner-only ability spec"), OwningClientIndex, [](FVelrathGasNetworkState& State)
            {
                return GetAbilitySpecCount(State) == 1 && HasTestAbility(State);
            })
            .ThenClient(TEXT("Validate owning client receives the ability spec"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsTrue(HasTestAbility(State), TEXT("Owning client must receive the granted Gameplay Ability spec.")));
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("Owning client must receive exactly one test ability spec.")));
                ASSERT_THAT(IsFalse(State.TargetCharacter->AreStartupAbilitiesGranted(), TEXT("Server-only grant bookkeeping must not replicate to the owning client.")));
            })
            .ThenClient(TEXT("Validate remote observer receives no owner-only ability spec"), RemoteClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsFalse(HasTestAbility(State), TEXT("Remote observer must not receive the owner's Gameplay Ability spec.")));
                ASSERT_THAT(AreEqual(0, GetAbilitySpecCount(State), TEXT("Remote observer ASC must expose no owner-only ability specs.")));
            })
            .ThenClient(TEXT("Attempt a client-side startup grant"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsFalse(State.TargetCharacter->GrantStartupAbilities(), TEXT("A client must not grant startup abilities.")));
            })
            .ThenServer(TEXT("Verify client grant attempt did not change authority"), [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("Client grant attempt must not change the server ASC.")));
                ASSERT_THAT(AreEqual(1, State.TargetCharacter->GetStartupAbilityHandleCount(), TEXT("Client grant attempt must not change authoritative handles.")));
            })
            .ThenClient(TEXT("Validate owning-client identity and ActorInfo"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                UAbilitySystemComponent* AbilitySystemComponent = State.TargetCharacter->GetAbilitySystemComponent();

                ASSERT_THAT(IsNotNull(LocalController, TEXT("Owning client must have a local PlayerController.")));
                ASSERT_THAT(IsTrue(LocalController->GetPawn() == State.TargetCharacter, TEXT("Owning client must possess the target pawn.")));
                ASSERT_THAT(IsTrue(AbilitySystemComponent->AbilityActorInfo.IsValid(), TEXT("Owning-client AbilityActorInfo must be valid.")));
                ASSERT_THAT(IsTrue(AbilitySystemComponent->AbilityActorInfo->PlayerController.Get() == LocalController, TEXT("Owning-client ActorInfo must reference its local PlayerController.")));
                ASSERT_THAT(AreEqual(EGameplayEffectReplicationMode::Mixed, AbilitySystemComponent->ReplicationMode, TEXT("Owning-client ASC must retain Mixed replication mode.")));
                ASSERT_THAT(IsFalse(State.TargetCharacter->AreDefaultAttributesInitialized(), TEXT("Owning client must not run server startup initialization.")));
                ASSERT_THAT(IsNear(InitialStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("Owning-client Stamina must initialize to 100.")));
                ASSERT_THAT(IsNear(InitialStamina, GetMaxStamina(State), UE_SMALL_NUMBER, TEXT("Owning-client MaxStamina must initialize to 100.")));
            })
            .ThenClient(TEXT("Validate remote-observer identity"), RemoteClientIndex, [this](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                ASSERT_THAT(IsNotNull(LocalController, TEXT("Remote client must have its own local PlayerController.")));
                ASSERT_THAT(IsTrue(LocalController->GetPawn() != State.TargetCharacter, TEXT("Remote client must observe, not own, the target pawn.")));
                ASSERT_THAT(IsNotNull(State.TargetCharacter->GetAbilitySystemComponent(), TEXT("Remote target pawn must expose its replicated ASC.")));
                ASSERT_THAT(AreEqual(EGameplayEffectReplicationMode::Mixed, State.TargetCharacter->GetAbilitySystemComponent()->ReplicationMode, TEXT("Remote ASC must retain Mixed replication mode.")));
                ASSERT_THAT(IsFalse(State.TargetCharacter->AreDefaultAttributesInitialized(), TEXT("Remote client must not run server startup initialization.")));
                ASSERT_THAT(IsNear(InitialStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("Remote-client Stamina must initialize to 100.")));
                ASSERT_THAT(IsNear(InitialStamina, GetMaxStamina(State), UE_SMALL_NUMBER, TEXT("Remote-client MaxStamina must initialize to 100.")));
            })
            .ThenServer(TEXT("Apply authoritative Health Gameplay Effect"), [](FVelrathGasNetworkState& State)
            {
                ApplyInstantOverride(
                    *State.TargetCharacter->GetAbilitySystemComponent(),
                    UVelrathAttributeSet::GetHealthAttribute(),
                    ReplicatedHealth);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for Health on owner and remote"), [](FVelrathGasNetworkState& State)
            {
                return GetHealth(State) == ReplicatedHealth;
            })
            .ThenServer(TEXT("Apply authoritative MaxHealth Gameplay Effect"), [](FVelrathGasNetworkState& State)
            {
                ApplyInstantOverride(
                    *State.TargetCharacter->GetAbilitySystemComponent(),
                    UVelrathAttributeSet::GetMaxHealthAttribute(),
                    ReplicatedMaxHealth);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for MaxHealth on owner and remote"), [](FVelrathGasNetworkState& State)
            {
                return GetMaxHealth(State) == ReplicatedMaxHealth && GetHealth(State) == ReplicatedHealth;
            })
            .ThenServer(TEXT("Apply authoritative Stamina Gameplay Effect"), [](FVelrathGasNetworkState& State)
            {
                ApplyInstantOverride(
                    *State.TargetCharacter->GetAbilitySystemComponent(),
                    UVelrathAttributeSet::GetStaminaAttribute(),
                    ReplicatedStamina);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for Stamina on owner and remote"), [](FVelrathGasNetworkState& State)
            {
                return GetStamina(State) == ReplicatedStamina;
            })
            .ThenServer(TEXT("Apply authoritative MaxStamina Gameplay Effect"), [](FVelrathGasNetworkState& State)
            {
                ApplyInstantOverride(
                    *State.TargetCharacter->GetAbilitySystemComponent(),
                    UVelrathAttributeSet::GetMaxStaminaAttribute(),
                    ReplicatedMaxStamina);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for MaxStamina on owner and remote"), [](FVelrathGasNetworkState& State)
            {
                return GetMaxStamina(State) == ReplicatedMaxStamina
                    && GetStamina(State) == ReplicatedStamina;
            })
            .ThenClient(TEXT("Attempt a client-only Health mutation"), OwningClientIndex, [](FVelrathGasNetworkState& State)
            {
                State.TargetCharacter->GetAbilitySystemComponent()->SetNumericAttributeBase(
                    UVelrathAttributeSet::GetHealthAttribute(), ClientOnlyHealth);
            })
            .ThenServer(TEXT("Verify client mutation did not reach authority"), [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsNear(ReplicatedHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("Owning-client mutation must not change authoritative Health.")));
            })
            .ThenClient(TEXT("Verify remote did not receive client mutation"), RemoteClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsNear(ReplicatedHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("Remote client must not receive another client's local Health mutation.")));
            })
            .ThenClient(TEXT("Verify mutation stayed local until correction"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsNear(ClientOnlyHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("The unauthorized write must remain only in the owning client's local world.")));
            })
            .ThenClient(TEXT("Attempt a client-only Stamina mutation"), OwningClientIndex, [](FVelrathGasNetworkState& State)
            {
                State.TargetCharacter->GetAbilitySystemComponent()->SetNumericAttributeBase(
                    UVelrathAttributeSet::GetStaminaAttribute(), ClientOnlyStamina);
            })
            .ThenServer(TEXT("Verify client Stamina mutation did not reach authority"), [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsNear(ReplicatedStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("Owning-client mutation must not change authoritative Stamina.")));
            })
            .ThenClient(TEXT("Verify remote did not receive client Stamina mutation"), RemoteClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsNear(ReplicatedStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("Remote client must not receive another client's local Stamina mutation.")));
            })
            .ThenClient(TEXT("Verify Stamina mutation stayed local until correction"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                ASSERT_THAT(IsNear(ClientOnlyStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("The unauthorized Stamina write must remain only in the owning client's local world.")));
            })
            .ThenServer(TEXT("Send an authoritative correction"), [](FVelrathGasNetworkState& State)
            {
                ApplyInstantOverride(
                    *State.TargetCharacter->GetAbilitySystemComponent(),
                    UVelrathAttributeSet::GetHealthAttribute(),
                    CorrectedHealth);
                ApplyInstantOverride(
                    *State.TargetCharacter->GetAbilitySystemComponent(),
                    UVelrathAttributeSet::GetStaminaAttribute(),
                    CorrectedStamina);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for authoritative attribute correction"), [](FVelrathGasNetworkState& State)
            {
                return GetHealth(State) == CorrectedHealth
                    && GetStamina(State) == CorrectedStamina;
            })
            .ThenServer(TEXT("Repossess the same pawn"), [this](FVelrathGasNetworkState& State)
            {
                AVelrathCharacter* Character = State.TargetCharacter;
                APlayerController* Controller = Cast<APlayerController>(Character->GetController());
                ASSERT_THAT(IsNotNull(Controller, TEXT("Target pawn must have its server PlayerController before repossession.")));

                Controller->UnPossess();
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("UnPossessed without destruction must preserve the startup ability.")));
                ASSERT_THAT(AreEqual(1, Character->GetStartupAbilityHandleCount(), TEXT("UnPossessed without destruction must preserve the startup handle.")));
                ASSERT_THAT(IsNear(CorrectedHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("UnPossessed without destruction must preserve Health.")));
                ASSERT_THAT(IsNear(CorrectedStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("UnPossessed without destruction must preserve Stamina.")));
                Controller->Possess(Character);

                ASSERT_THAT(IsTrue(Controller->GetPawn() == Character, TEXT("Server must repossess the same pawn.")));
                ASSERT_THAT(IsNear(CorrectedHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("ActorInfo reinitialization must not reset Health.")));
                ASSERT_THAT(IsNear(CorrectedStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("ActorInfo reinitialization must not reset Stamina.")));
                ASSERT_THAT(IsNear(ReplicatedMaxStamina, GetMaxStamina(State), UE_SMALL_NUMBER, TEXT("ActorInfo reinitialization must not reset MaxStamina.")));
                ASSERT_THAT(IsTrue(HasReadyGasState(Character), TEXT("Server ActorInfo must remain valid after repossession.")));
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("Repossession must not duplicate the startup ability.")));
                ASSERT_THAT(AreEqual(1, Character->GetStartupAbilityHandleCount(), TEXT("Repossession must retain exactly one startup handle.")));
            })
            .UntilClient(TEXT("Wait for owning client after repossession"), OwningClientIndex, [](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                return LocalController != nullptr
                    && LocalController->GetPawn() == State.TargetCharacter
                    && HasReadyGasState(State.TargetCharacter)
                    && GetHealth(State) == CorrectedHealth
                    && GetStamina(State) == CorrectedStamina
                    && GetMaxStamina(State) == ReplicatedMaxStamina
                    && GetAbilitySpecCount(State) == 1
                    && HasTestAbility(State);
            })
            .ThenClients(TEXT("Remember old pawn before replacement"), [](FVelrathGasNetworkState& State)
            {
                State.PreviousTargetCharacter = State.TargetCharacter;
                State.PreviousAttributeSet = State.TargetCharacter->GetVelrathAttributeSet();
                State.PreviousAbilitySystemComponent = State.TargetCharacter->GetAbilitySystemComponent();
            })
            .ThenServer(TEXT("Destroy and replace the authoritative pawn"), [this](FVelrathGasNetworkState& State)
            {
                State.PreviousTargetCharacter = State.TargetCharacter;
                State.PreviousAttributeSet = State.TargetCharacter->GetVelrathAttributeSet();
                UAbilitySystemComponent* PreviousAbilitySystemComponent =
                    State.TargetCharacter->GetAbilitySystemComponent();
                State.PreviousAbilitySystemComponent = PreviousAbilitySystemComponent;
                APlayerController* Controller = Cast<APlayerController>(State.TargetCharacter->GetController());
                AGameModeBase* GameMode = State.World->GetAuthGameMode();
                ASSERT_THAT(IsNotNull(Controller, TEXT("Replacement requires the target server PlayerController.")));
                ASSERT_THAT(IsNotNull(GameMode, TEXT("Replacement requires the authoritative GameMode.")));

                Controller->UnPossess();
                State.PreviousTargetCharacter->Destroy();
                ASSERT_THAT(IsTrue(PreviousAbilitySystemComponent->GetOwnerActor() == nullptr, TEXT("Destroyed server Character must clear the old ASC owner.")));
                ASSERT_THAT(IsTrue(PreviousAbilitySystemComponent->GetAvatarActor() == nullptr, TEXT("Destroyed server Character must clear the old ASC avatar.")));
                ASSERT_THAT(AreEqual(0, PreviousAbilitySystemComponent->GetNumActiveGameplayEffects(), TEXT("Destroyed server Character must retire all active effects.")));
                ASSERT_THAT(AreEqual(0, PreviousAbilitySystemComponent->GetActivatableAbilities().Num(), TEXT("Destroyed server Character must leave no activatable abilities.")));
                ASSERT_THAT(AreEqual(0, State.PreviousTargetCharacter->GetStartupAbilityHandleCount(), TEXT("Destroyed server Character must clear its retained startup handles.")));
                GameMode->RestartPlayer(Controller);
                State.TargetCharacter = Cast<AVelrathCharacter>(Controller->GetPawn());

                ASSERT_THAT(IsNotNull(State.TargetCharacter, TEXT("GameMode must create a replacement VELRATH Character.")));
                ASSERT_THAT(IsTrue(State.TargetCharacter != State.PreviousTargetCharacter, TEXT("Replacement must be a new Character instance.")));
                ASSERT_THAT(IsTrue(State.TargetCharacter->GetVelrathAttributeSet() != State.PreviousAttributeSet, TEXT("Replacement Character must own a new AttributeSet instance.")));
                ASSERT_THAT(IsTrue(HasReadyGasState(State.TargetCharacter), TEXT("Replacement server Character must have valid GAS ActorInfo.")));
                ASSERT_THAT(IsTrue(State.TargetCharacter->AreDefaultAttributesInitialized(), TEXT("Replacement server Character must run one fresh initialization effect.")));
                ASSERT_THAT(IsNear(InitialHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("Replacement server Character must initialize Health to 1000.")));
                ASSERT_THAT(IsNear(InitialHealth, GetMaxHealth(State), UE_SMALL_NUMBER, TEXT("Replacement server Character must initialize MaxHealth to 1000.")));
                ASSERT_THAT(IsNear(InitialStamina, GetStamina(State), UE_SMALL_NUMBER, TEXT("Replacement server Character must initialize Stamina to 100.")));
                ASSERT_THAT(IsNear(InitialStamina, GetMaxStamina(State), UE_SMALL_NUMBER, TEXT("Replacement server Character must initialize MaxStamina to 100.")));

                UVelrathAbilitySet* TestSet = MakeNetworkTestAbilitySet(*State.TargetCharacter);
                ASSERT_THAT(IsNotNull(TestSet, TEXT("Replacement network test must create a transient Ability Set.")));
                ASSERT_THAT(IsTrue(State.TargetCharacter->RemoveStartupAbilities(), TEXT("Replacement must clear its completed empty production grant state.")));
                State.TargetCharacter->SetStartupAbilitySet(TestSet);
                ASSERT_THAT(IsTrue(State.TargetCharacter->GrantStartupAbilities(), TEXT("Replacement Character must receive its own test grant.")));
                ASSERT_THAT(AreEqual(1, State.TargetCharacter->GetStartupAbilityHandleCount(), TEXT("Replacement Character must retain its own valid handle.")));
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("Replacement server ASC must contain one test ability.")));
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for replacement pawn on owner and remote"), [this](FVelrathGasNetworkState& State)
            {
                AVelrathCharacter* Replacement = FindCharacterForPlayerId(*State.World, TargetPlayerId);
                if (Replacement == nullptr || Replacement == State.PreviousTargetCharacter)
                {
                    return false;
                }

                State.TargetCharacter = Replacement;
                const UAbilitySystemComponent* PreviousAbilitySystemComponent =
                    State.PreviousAbilitySystemComponent.Get();
                const bool bPreviousAbilitySystemRetired = PreviousAbilitySystemComponent == nullptr ||
                    (PreviousAbilitySystemComponent->GetOwnerActor() == nullptr &&
                        PreviousAbilitySystemComponent->GetAvatarActor() == nullptr);
                return HasReadyGasState(Replacement)
                    && bPreviousAbilitySystemRetired
                    && Replacement->GetVelrathAttributeSet() != State.PreviousAttributeSet
                    && GetHealth(State) == InitialHealth
                    && GetMaxHealth(State) == InitialHealth
                    && GetStamina(State) == InitialStamina
                    && GetMaxStamina(State) == InitialStamina
                    && (State.ClientIndex == OwningClientIndex
                        ? GetAbilitySpecCount(State) == 1 && HasTestAbility(State)
                        : GetAbilitySpecCount(State) == 0 && !HasTestAbility(State));
            })
            .ThenClient(TEXT("Validate replacement owning-client ActorInfo"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                UAbilitySystemComponent* AbilitySystemComponent = State.TargetCharacter->GetAbilitySystemComponent();
                ASSERT_THAT(IsTrue(LocalController != nullptr && LocalController->GetPawn() == State.TargetCharacter, TEXT("Owning client must possess the replacement Character.")));
                ASSERT_THAT(IsTrue(AbilitySystemComponent->AbilityActorInfo.IsValid(), TEXT("Replacement owning-client ActorInfo must be valid.")));
                ASSERT_THAT(IsTrue(AbilitySystemComponent->AbilityActorInfo->PlayerController.Get() == LocalController, TEXT("Replacement ActorInfo must reference the owning PlayerController.")));
                ASSERT_THAT(IsTrue(HasTestAbility(State), TEXT("Replacement owning client must receive the replacement pawn's ability spec.")));
                ASSERT_THAT(AreEqual(1, GetAbilitySpecCount(State), TEXT("Replacement owning client must receive exactly one ability spec.")));
            })
            .Then(TEXT("Record completed multi-instance scenario"), [this]()
            {
                TestRunner->AddInfo(TEXT("GAS-009 PASS: listen server + 2 clients independently verified server-only retirement, idempotent grants, owner-only spec delivery, UnPossessed/repossession preservation, old-pawn cleanup, pawn replacement, and attribute regression."));
                UE_LOG(LogVelrathGasNetworkTest, Display,
                    TEXT("GAS-009 PASS: the old pawn retired cleanly and the replacement ASC, grants, and attributes converged with the expected owner/remote visibility."));
            });
    }
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
