#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VelrathAttributeSet.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"
#include "Game/VelrathGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/VelrathCharacter.h"

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

struct FVelrathGasNetworkState : public FBasePIENetworkComponentState
{
    AVelrathCharacter* TargetCharacter = nullptr;
    AVelrathCharacter* PreviousTargetCharacter = nullptr;
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

float GetHealth(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetVelrathAttributeSet()->GetHealth();
}

float GetMaxHealth(const FVelrathGasNetworkState& State)
{
    return State.TargetCharacter->GetVelrathAttributeSet()->GetMaxHealth();
}
}

NETWORK_TEST_CLASS(FVelrathGasReplicationNetworkTest, "VELRATH.Network.GAS")
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
                    && GetMaxHealth(State) == InitialHealth;
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
                ASSERT_THAT(IsNear(InitialHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("Server Health must initialize to 1000.")));
                ASSERT_THAT(IsNear(InitialHealth, GetMaxHealth(State), UE_SMALL_NUMBER, TEXT("Server MaxHealth must initialize to 1000.")));

                UE_LOG(LogVelrathGasNetworkTest, Display,
                    TEXT("NET-TEST-001 initial state: serverNetMode=%d clients=%d targetPlayerId=%d Health=%.0f MaxHealth=%.0f"),
                    static_cast<int32>(State.World->GetNetMode()), State.ClientCount, TargetPlayerId,
                    GetHealth(State), GetMaxHealth(State));
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
            })
            .ThenClient(TEXT("Validate remote-observer identity"), RemoteClientIndex, [this](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                ASSERT_THAT(IsNotNull(LocalController, TEXT("Remote client must have its own local PlayerController.")));
                ASSERT_THAT(IsTrue(LocalController->GetPawn() != State.TargetCharacter, TEXT("Remote client must observe, not own, the target pawn.")));
                ASSERT_THAT(IsNotNull(State.TargetCharacter->GetAbilitySystemComponent(), TEXT("Remote target pawn must expose its replicated ASC.")));
                ASSERT_THAT(AreEqual(EGameplayEffectReplicationMode::Mixed, State.TargetCharacter->GetAbilitySystemComponent()->ReplicationMode, TEXT("Remote ASC must retain Mixed replication mode.")));
            })
            .ThenServer(TEXT("Set authoritative Health"), [](FVelrathGasNetworkState& State)
            {
                State.TargetCharacter->GetAbilitySystemComponent()->SetNumericAttributeBase(
                    UVelrathAttributeSet::GetHealthAttribute(), ReplicatedHealth);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for Health on owner and remote"), [](FVelrathGasNetworkState& State)
            {
                return GetHealth(State) == ReplicatedHealth;
            })
            .ThenServer(TEXT("Set authoritative MaxHealth"), [](FVelrathGasNetworkState& State)
            {
                State.TargetCharacter->GetAbilitySystemComponent()->SetNumericAttributeBase(
                    UVelrathAttributeSet::GetMaxHealthAttribute(), ReplicatedMaxHealth);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for MaxHealth on owner and remote"), [](FVelrathGasNetworkState& State)
            {
                return GetMaxHealth(State) == ReplicatedMaxHealth && GetHealth(State) == ReplicatedHealth;
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
            .ThenServer(TEXT("Send an authoritative correction"), [](FVelrathGasNetworkState& State)
            {
                State.TargetCharacter->GetAbilitySystemComponent()->SetNumericAttributeBase(
                    UVelrathAttributeSet::GetHealthAttribute(), CorrectedHealth);
                State.TargetCharacter->ForceNetUpdate();
            })
            .UntilClients(TEXT("Wait for authoritative Health correction"), [](FVelrathGasNetworkState& State)
            {
                return GetHealth(State) == CorrectedHealth;
            })
            .ThenServer(TEXT("Repossess the same pawn"), [this](FVelrathGasNetworkState& State)
            {
                AVelrathCharacter* Character = State.TargetCharacter;
                APlayerController* Controller = Cast<APlayerController>(Character->GetController());
                ASSERT_THAT(IsNotNull(Controller, TEXT("Target pawn must have its server PlayerController before repossession.")));

                Controller->UnPossess();
                Controller->Possess(Character);

                ASSERT_THAT(IsTrue(Controller->GetPawn() == Character, TEXT("Server must repossess the same pawn.")));
                ASSERT_THAT(IsNear(CorrectedHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("ActorInfo reinitialization must not reset Health.")));
                ASSERT_THAT(IsTrue(HasReadyGasState(Character), TEXT("Server ActorInfo must remain valid after repossession.")));
            })
            .UntilClient(TEXT("Wait for owning client after repossession"), OwningClientIndex, [](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                return LocalController != nullptr
                    && LocalController->GetPawn() == State.TargetCharacter
                    && HasReadyGasState(State.TargetCharacter)
                    && GetHealth(State) == CorrectedHealth;
            })
            .ThenClients(TEXT("Remember old pawn before replacement"), [](FVelrathGasNetworkState& State)
            {
                State.PreviousTargetCharacter = State.TargetCharacter;
            })
            .ThenServer(TEXT("Destroy and replace the authoritative pawn"), [this](FVelrathGasNetworkState& State)
            {
                State.PreviousTargetCharacter = State.TargetCharacter;
                APlayerController* Controller = Cast<APlayerController>(State.TargetCharacter->GetController());
                AGameModeBase* GameMode = State.World->GetAuthGameMode();
                ASSERT_THAT(IsNotNull(Controller, TEXT("Replacement requires the target server PlayerController.")));
                ASSERT_THAT(IsNotNull(GameMode, TEXT("Replacement requires the authoritative GameMode.")));

                Controller->UnPossess();
                State.PreviousTargetCharacter->Destroy();
                GameMode->RestartPlayer(Controller);
                State.TargetCharacter = Cast<AVelrathCharacter>(Controller->GetPawn());

                ASSERT_THAT(IsNotNull(State.TargetCharacter, TEXT("GameMode must create a replacement VELRATH Character.")));
                ASSERT_THAT(IsTrue(State.TargetCharacter != State.PreviousTargetCharacter, TEXT("Replacement must be a new Character instance.")));
                ASSERT_THAT(IsTrue(HasReadyGasState(State.TargetCharacter), TEXT("Replacement server Character must have valid GAS ActorInfo.")));
                ASSERT_THAT(IsNear(InitialHealth, GetHealth(State), UE_SMALL_NUMBER, TEXT("Replacement server Character must initialize Health to 1000.")));
                ASSERT_THAT(IsNear(InitialHealth, GetMaxHealth(State), UE_SMALL_NUMBER, TEXT("Replacement server Character must initialize MaxHealth to 1000.")));
            })
            .UntilClients(TEXT("Wait for replacement pawn on owner and remote"), [this](FVelrathGasNetworkState& State)
            {
                AVelrathCharacter* Replacement = FindCharacterForPlayerId(*State.World, TargetPlayerId);
                if (Replacement == nullptr || Replacement == State.PreviousTargetCharacter)
                {
                    return false;
                }

                State.TargetCharacter = Replacement;
                return HasReadyGasState(Replacement)
                    && GetHealth(State) == InitialHealth
                    && GetMaxHealth(State) == InitialHealth;
            })
            .ThenClient(TEXT("Validate replacement owning-client ActorInfo"), OwningClientIndex, [this](FVelrathGasNetworkState& State)
            {
                APlayerController* LocalController = State.World->GetFirstPlayerController();
                UAbilitySystemComponent* AbilitySystemComponent = State.TargetCharacter->GetAbilitySystemComponent();
                ASSERT_THAT(IsTrue(LocalController != nullptr && LocalController->GetPawn() == State.TargetCharacter, TEXT("Owning client must possess the replacement Character.")));
                ASSERT_THAT(IsTrue(AbilitySystemComponent->AbilityActorInfo.IsValid(), TEXT("Replacement owning-client ActorInfo must be valid.")));
                ASSERT_THAT(IsTrue(AbilitySystemComponent->AbilityActorInfo->PlayerController.Get() == LocalController, TEXT("Replacement ActorInfo must reference the owning PlayerController.")));
            })
            .Then(TEXT("Record completed multi-instance scenario"), [this]()
            {
                TestRunner->AddInfo(TEXT("NET-TEST-001 PASS: listen server + 2 clients independently verified owner/remote GAS attribute replication, client non-authority, ActorInfo reinitialization, and pawn replacement."));
                UE_LOG(LogVelrathGasNetworkTest, Display,
                    TEXT("NET-TEST-001 PASS: Health/MaxHealth converged across server, owning client, and remote client; replacement pawn initialized cleanly."));
            });
    }
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
