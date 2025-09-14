#include "DirectorGameState.h"
#include "Net/UnrealNetwork.h"   // <-- required for DOREPLIFETIME
#include "CameraRig.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogDirectorGS, Log, All);


ADirectorGameState::ADirectorGameState()
{
    bReplicates = true;
    ActiveCamera = nullptr; 
}

void ADirectorGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADirectorGameState, ActiveCamera);
    DOREPLIFETIME(ADirectorGameState, OperatorPlayerState);
}


void ADirectorGameState::OnRep_ActiveCamera()
{
    // Ensure late-joining clients start capturing from the active camera
    if (ActiveCamera && ActiveCamera->SceneCapture)
    {
        if (ActiveCamera->RenderTarget)
        {
            ActiveCamera->SceneCapture->TextureTarget = ActiveCamera->RenderTarget;
            ActiveCamera->SceneCapture->bCaptureEveryFrame = true;
            ActiveCamera->SceneCapture->CaptureScene();
            UE_LOG(LogDirectorGS, Log, TEXT("[GS] OnRep ActiveCamera=%s (RT=%s)"), *ActiveCamera->GetName(), *GetNameSafe(ActiveCamera->RenderTarget));

            // Friendly on-screen cue so it's clear which camera is now active
            if (GEngine)
            {
                FString RigName;
                if (ActiveCamera->RigLabel != NAME_None)
                {
                    RigName = ActiveCamera->RigLabel.ToString();
                }
#if WITH_EDITOR
                if (RigName.IsEmpty())
                {
                    RigName = ActiveCamera->GetActorLabel();
                }
#endif
                if (RigName.IsEmpty())
                {
                    RigName = ActiveCamera->GetName();
                }
                const FString Msg = FString::Printf(TEXT("Switched to :  %s"), *RigName);
                GEngine->AddOnScreenDebugMessage(770778, 2.5f, FColor::Cyan, Msg);
            }
        }
    }
    else
    {
        UE_LOG(LogDirectorGS, Log, TEXT("[GS] OnRep ActiveCamera=None"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(770778, 2.0f, FColor::Silver, TEXT("Active camera: none"));
        }
    }
    OnActiveCameraChanged.Broadcast(ActiveCamera);
}

UTextureRenderTarget2D* ADirectorGameState::GetActiveCameraRenderTarget() const
{
    return ActiveCamera ? ActiveCamera->RenderTarget : nullptr;
}

void ADirectorGameState::OnRep_Operator()
{
    OnOperatorChanged.Broadcast(OperatorPlayerState);
}
