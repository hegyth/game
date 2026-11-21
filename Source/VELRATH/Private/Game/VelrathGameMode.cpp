#include "Game/VelrathGameMode.h"

#include "Game/VelrathGameState.h"
#include "Player/VelrathCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVelrathGameMode::AVelrathGameMode()
{
    GameStateClass = AVelrathGameState::StaticClass();

    static ConstructorHelpers::FClassFinder<AVelrathCharacter> CharacterBlueprint(
        TEXT("/Game/Characters/BP_VelrathCharacter"));

    DefaultPawnClass = AVelrathCharacter::StaticClass();
    if (CharacterBlueprint.Succeeded())
    {
        DefaultPawnClass = CharacterBlueprint.Class;
    }
}

