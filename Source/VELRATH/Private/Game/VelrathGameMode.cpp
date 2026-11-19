#include "Game/VelrathGameMode.h"

#include "Player/VelrathCharacter.h"

AVelrathGameMode::AVelrathGameMode()
{
    DefaultPawnClass = AVelrathCharacter::StaticClass();
}

