// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonCameraManGameMode.h"
#include "ThirdPersonCameraManCharacter.h"          // your pawn class
#include "ThirdPersonCameraManPlayerController.h" 
#include "DirectorGameState.h"

AThirdPersonCameraManGameMode::AThirdPersonCameraManGameMode()
{
	DefaultPawnClass      = AThirdPersonCameraManCharacter::StaticClass();
	PlayerControllerClass = AThirdPersonCameraManPlayerController::StaticClass();
	GameStateClass        = ADirectorGameState::StaticClass();
}

void AThirdPersonCameraManGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (HasAuthority() && !OperatorPC)
    {
        OperatorPC = NewPlayer;
        if (ADirectorGameState* GS = GetGameState<ADirectorGameState>())
        {
            GS->OperatorPlayerState = NewPlayer->PlayerState;
        }
    }
}

void AThirdPersonCameraManGameMode::SetActiveCamera(ACameraRig* NewActive)
{
    if (!HasAuthority()) return;
    if (auto* GS = Cast<ADirectorGameState>(GameState)) GS->ActiveCamera = NewActive;
}

void AThirdPersonCameraManGameMode::ClearActiveCamera()
{
    if (!HasAuthority()) return;
    if (auto* GS = Cast<ADirectorGameState>(GameState)) GS->ActiveCamera = nullptr;
}

void AThirdPersonCameraManGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Let base do its normal init and spawn; we'll clean up viewers after
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    if (!HasAuthority() || !NewPlayer)
    {
        return;
    }

    if (!OperatorPC)
    {
        // First player becomes operator
        OperatorPC = NewPlayer;
        if (ADirectorGameState* GS = GetGameState<ADirectorGameState>())
        {
            GS->OperatorPlayerState = NewPlayer->PlayerState;
        }
        return; // keep their pawn
    }

    // Everyone else is a viewer: destroy any spawned pawn and put them in spectate
    if (APawn* P = NewPlayer->GetPawn())
    {
        P->Destroy();
    }
    NewPlayer->StartSpectatingOnly();
}
