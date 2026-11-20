#include "Game/VelrathGameMode.h"

#include "Player/VelrathCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVelrathGameMode::AVelrathGameMode()
{
    static ConstructorHelpers::FClassFinder<AVelrathCharacter> CharacterBlueprint(
        TEXT("/Game/Characters/BP_VelrathCharacter"));

    DefaultPawnClass = AVelrathCharacter::StaticClass();
    if (CharacterBlueprint.Succeeded())
    {
        DefaultPawnClass = CharacterBlueprint.Class;
    }
}

