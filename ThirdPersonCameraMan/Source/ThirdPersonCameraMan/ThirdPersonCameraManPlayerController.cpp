// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonCameraManPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "CameraRig.h"
#include "DirectorGameState.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "ThirdPersonCameraMan.h"
#include "Engine/Engine.h"
#include "ThirdPersonCameraManGameMode.h"
#include "Components/SceneCaptureComponent2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogDirectorPC, Log, All);
#include "Widgets/Input/SVirtualJoystick.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerState.h"

void AThirdPersonCameraManPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("[PC %s] BeginPlay IsLocal=%d"),
        *GetName(), IsLocalController());

    if (!IsLocalController())
    {
        return;
    }

    if (ADirectorGameState* GS = GetWorld()->GetGameState<ADirectorGameState>())
    {
        GS->OnActiveCameraChanged.AddUObject(
            this, &AThirdPersonCameraManPlayerController::HandleActiveCameraChanged);
        HandleActiveCameraChanged(GS->ActiveCamera); // initial sync

        GS->OnOperatorChanged.AddUObject(this, &AThirdPersonCameraManPlayerController::HandleOperatorChanged);
        RefreshLocalRoleFromGameState();
        UpdateInputMappingsForRole();

        UE_LOG(LogDirectorPC, Log, TEXT("[PC %s] RoleResolved=%d IsOperator=%d ActiveCam=%s"),
            *GetName(), bRoleResolved ? 1 : 0, bIsOperator ? 1 : 0,
            GS->ActiveCamera ? *GS->ActiveCamera->GetName() : TEXT("None"));
    }

    // Setup overlay widget (PiP) depending on role
    if (IsLocalPlayerController())
    {
        EnsureCameraFeedWidget();
        UE_LOG(LogDirectorPC, Log, TEXT("[PC %s] Overlay Operator=%d Viewer=%d Class=%s"),
            *GetName(), bOverlayForOperator ? 1 : 0, bOverlayForViewer ? 1 : 0,
            CameraFeedClass ? *CameraFeedClass->GetName() : TEXT("None"));

        // Only spawn touch controls for operator (who actually plays)
        if (bIsOperator && SVirtualJoystick::ShouldDisplayTouchInterface())
        {
            MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
            if (MobileControlsWidget)
            {
                MobileControlsWidget->AddToPlayerScreen(0);
            }
            else
            {
                UE_LOG(LogThirdPersonCameraMan, Error, TEXT("Could not spawn mobile controls widget."));
            }
        }
    }
}

// Viewer camera: set view target to active rig; return to pawn when cleared
void AThirdPersonCameraManPlayerController::HandleActiveCameraChanged(ACameraRig* NewCam)
{
    // If using UI feed, keep it in sync (allow local override)
    UTextureRenderTarget2D* RT = FeedOverrideRT ? FeedOverrideRT : (NewCam ? NewCam->RenderTarget : nullptr);
    CallWidgetSetFeedRT(RT);

    // If we're a viewer (non-operator), drive the actual camera view instead of a widget
    if (IsLocalController() && (!bIsOperator || bForceViewFromActiveRig))
    {
        if (NewCam)
        {
            UE_LOG(LogDirectorPC, Log, TEXT("[PC %s] Viewer switching to ActiveCamera=%s"), *GetName(), *NewCam->GetName());
            SetViewTargetWithBlend(NewCam, 0.25f);
        }
        else
        {
            // No active camera anymore (dropped) → return viewer to their own pawn
            if (APawn* P = GetPawn())
            {
                UE_LOG(LogDirectorPC, Log, TEXT("[PC %s] ActiveCamera cleared; returning view to pawn %s"), *GetName(), *P->GetName());
                SetViewTargetWithBlend(P, 0.25f);
            }
        }
    }
}

void AThirdPersonCameraManPlayerController::RigForceView(bool bEnable)
{
    if (!IsLocalController()) return;
    bForceViewFromActiveRig = bEnable;

    const ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS) return;

    if (bForceViewFromActiveRig && GS->ActiveCamera)
    {
        SetViewTargetWithBlend(GS->ActiveCamera, 0.2f);
    }
    else if (APawn* P = GetPawn())
    {
        SetViewTargetWithBlend(P, 0.2f);
    }
}

void AThirdPersonCameraManPlayerController::CallWidgetSetFeedRT(UTextureRenderTarget2D* RT)
{
    if (!CameraFeed) return;
    if (!RT)
    {
        // Render target might not be created on this client yet; retry shortly
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(FeedRetryHandle);
            World->GetTimerManager().SetTimer(FeedRetryHandle, this,
                &AThirdPersonCameraManPlayerController::RetryApplyFeedRT, 0.05f, false);
        }
        return;
    }

    static const FName FnName(TEXT("SetFeedRT")); // must exist in your BP widget
    if (UFunction* Fn = CameraFeed->FindFunction(FnName))
    {
        struct { UTextureRenderTarget2D* RenderTarget; } Params{ RT };
        CameraFeed->ProcessEvent(Fn, &Params);
    }
    else
    {
        UE_LOG(LogDirectorPC, Warning, TEXT("[PC %s] CameraFeed widget '%s' missing SetFeedRT(UTextureRenderTarget2D*)"),
            *GetName(), *CameraFeed->GetName());
    }
}

void AThirdPersonCameraManPlayerController::RetryApplyFeedRT()
{
    const ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS) return;
    CallWidgetSetFeedRT(GS->GetActiveCameraRenderTarget());
}

void AThirdPersonCameraManPlayerController::RefreshLocalRoleFromGameState()
{
    bIsOperator = false;
    bRoleResolved = false;
    if (!IsLocalController()) return;
    if (const ADirectorGameState* GS = GetWorld()->GetGameState<ADirectorGameState>())
    {
        if (GS->OperatorPlayerState)
        {
            bRoleResolved = true;
            if (GS->OperatorPlayerState == PlayerState)
            {
                bIsOperator = true;
            }
        }
    }
}

void AThirdPersonCameraManPlayerController::HandleOperatorChanged(APlayerState* NewOperator)
{
    const bool bWasOperator = bIsOperator;
    RefreshLocalRoleFromGameState();

    if (IsLocalController() && bWasOperator != bIsOperator)
    {
        // If roles flipped at runtime (rare), adjust view and overlay accordingly
        if (!bIsOperator)
        {
            EnsureCameraFeedWidget();
            // Viewers should view through the active camera rig
            if (const ADirectorGameState* GS = GetWorld()->GetGameState<ADirectorGameState>())
            {
                if (GS->ActiveCamera)
                {
                    SetViewTargetWithBlend(GS->ActiveCamera, 0.25f);
                }
            }
        }
        else
        {
            EnsureCameraFeedWidget();
            // Operators should return to their own pawn view if available
            if (APawn* P = GetPawn())
            {
                SetViewTargetWithBlend(P, 0.25f);
            }
        }

        UpdateInputMappingsForRole();
        ShowRoleLabel();
    }
    else if (IsLocalController())
    {
        // Even if role didn't flip, we might have just resolved roles; ensure mappings are consistent now
        UpdateInputMappingsForRole();
    }
}

void AThirdPersonCameraManPlayerController::EnsureCameraFeedWidget()
{
    if (!IsLocalPlayerController()) return;

    const bool bWantsOverlay = bIsOperator ? bOverlayForOperator : bOverlayForViewer;

    if (!bWantsOverlay)
    {
        if (CameraFeed)
        {
            CameraFeed->RemoveFromParent();
            CameraFeed = nullptr;
        }
        return;
    }

    if (!CameraFeed && CameraFeedClass)
    {
        CameraFeed = CreateWidget<UUserWidget>(this, CameraFeedClass);
        if (CameraFeed)
        {
            CameraFeed->AddToViewport(OverlayZOrder);
            UpdateFeedOverlayLayout();
            // Start hidden; FeedToggle() will reveal when requested
            CameraFeed->SetVisibility(ESlateVisibility::Collapsed);

            // Apply current RT if available
            if (const ADirectorGameState* GS = GetWorld()->GetGameState<ADirectorGameState>())
            {
                CallWidgetSetFeedRT(GS->GetActiveCameraRenderTarget());
            }
        }
    }
    else if (CameraFeed)
    {
        UpdateFeedOverlayLayout();
    }
    else if (!CameraFeedClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PC %s] CameraFeedClass not set; overlay disabled."), *GetName());
    }
}

void AThirdPersonCameraManPlayerController::UpdateFeedOverlayLayout()
{
    if (!CameraFeed) return;

    // Compute absolute position from viewport size and desired corner
    const FVector2D ViewSize = UWidgetLayoutLibrary::GetViewportSize(this);
    FVector2D Pos(OverlayMargin.X, OverlayMargin.Y);

    switch (OverlayCorner)
    {
    case EFeedCorner::TopLeft:
        Pos = FVector2D(OverlayMargin.X, OverlayMargin.Y);
        break;
    case EFeedCorner::TopRight:
        Pos = FVector2D(ViewSize.X - OverlaySize.X - OverlayMargin.X, OverlayMargin.Y);
        break;
    case EFeedCorner::BottomLeft:
        Pos = FVector2D(OverlayMargin.X, ViewSize.Y - OverlaySize.Y - OverlayMargin.Y);
        break;
    case EFeedCorner::BottomRight:
    default:
        Pos = FVector2D(ViewSize.X - OverlaySize.X - OverlayMargin.X,
                        ViewSize.Y - OverlaySize.Y - OverlayMargin.Y);
        break;
    }

    CameraFeed->SetAlignmentInViewport(FVector2D(0.f, 0.f));
    CameraFeed->SetDesiredSizeInViewport(OverlaySize);
    CameraFeed->SetPositionInViewport(Pos, false);
}

// --- Console helpers to quickly calibrate the active rig ---
void AThirdPersonCameraManPlayerController::RigCycleAxis()
{
    if (!IsLocalController()) return;
    ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS || !GS->ActiveCamera) return;

    using EAxis = EAssetForwardAxis;
    int32 Val = static_cast<int32>(GS->ActiveCamera->AssetForwardAxis);
    Val = (Val + 1) % 6;
    GS->ActiveCamera->AssetForwardAxis = static_cast<EAxis>(Val);
    GS->ActiveCamera->ReapplyViewAlignment(nullptr);
    GS->ActiveCamera->ApplyLocalOffsets();
    UE_LOG(LogTemp, Log, TEXT("[RigCycleAxis] Now %d"), Val);
}

void AThirdPersonCameraManPlayerController::RigNudge(float Yaw, float Pitch, float Roll)
{
    if (!IsLocalController()) return;
    ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS || !GS->ActiveCamera) return;
    GS->ActiveCamera->AlignYawOffsetDeg   += Yaw;
    GS->ActiveCamera->AlignPitchOffsetDeg += Pitch;
    GS->ActiveCamera->AlignRollOffsetDeg  += Roll;
    GS->ActiveCamera->ReapplyViewAlignment(nullptr);
    UE_LOG(LogTemp, Log, TEXT("[RigNudge] Yaw=%.1f Pitch=%.1f Roll=%.1f"), Yaw, Pitch, Roll);
}

void AThirdPersonCameraManPlayerController::RigZeroRoll(bool bZero)
{
    if (!IsLocalController()) return;
    ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS || !GS->ActiveCamera) return;
    GS->ActiveCamera->bZeroRollOnAttach = bZero;
    GS->ActiveCamera->ReapplyViewAlignment(nullptr);
    UE_LOG(LogTemp, Log, TEXT("[RigZeroRoll] %d"), bZero ? 1 : 0);
}

void AThirdPersonCameraManPlayerController::RigPrint()
{
    if (!IsLocalController() || !GEngine) return;
    ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS || !GS->ActiveCamera) return;
    const ACameraRig* R = GS->ActiveCamera;
    const FString Msg = FString::Printf(TEXT("Axis=%d Yaw=%.1f Pitch=%.1f Roll=%.1f ZeroRoll=%d CamRel=(%s)"),
        (int32)R->AssetForwardAxis, R->AlignYawOffsetDeg, R->AlignPitchOffsetDeg, R->AlignRollOffsetDeg,
        R->bZeroRollOnAttach ? 1 : 0,
        *R->CameraRelativeRotation.ToString());
    GEngine->AddOnScreenDebugMessage(770099, 5.f, FColor::Yellow, Msg);
}

// --- Secondary feed helpers (toggle/cycle from code) ---
void AThirdPersonCameraManPlayerController::FeedSetRigByIndex(int32 Index)
{
    if (!IsLocalController()) return;
    UWorld* World = GetWorld();
    if (!World) return;

    int32 i = 0;
    for (TActorIterator<ACameraRig> It(World); It; ++It)
    {
        if (i == Index)
        {
            if (ACameraRig* Rig = *It)
            {
                if (Rig->RenderTarget)
                {
                    FeedOverrideRT = Rig->RenderTarget;
                    CallWidgetSetFeedRT(FeedOverrideRT);
                }
            }
            return;
        }
        ++i;
    }
}

void AThirdPersonCameraManPlayerController::FeedClearOverride()
{
    if (!IsLocalController()) return;
    FeedOverrideRT = nullptr;
    const ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    CallWidgetSetFeedRT(GS ? GS->GetActiveCameraRenderTarget() : nullptr);
}

void AThirdPersonCameraManPlayerController::FeedToggle()
{
    if (!IsLocalController()) return;
    EnsureCameraFeedWidget();
    if (!CameraFeed) return;
    const ESlateVisibility Vis = CameraFeed->GetVisibility();
    const bool bIsHidden = (Vis == ESlateVisibility::Collapsed || Vis == ESlateVisibility::Hidden);

    if (bIsHidden)
    {
        // Only show if this player has picked up a camera rig (is operator and there is an active rig)
        const ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
        const bool bCanShow = (bIsOperator && GS && GS->ActiveCamera && GS->GetActiveCameraRenderTarget());
        if (!bCanShow)
        {
            UE_LOG(LogTemp, Log, TEXT("[PC %s] FeedToggle ignored (no active rig on this player)."), *GetName());
            return;
        }

        UTextureRenderTarget2D* RT = FeedOverrideRT ? FeedOverrideRT : GS->GetActiveCameraRenderTarget();
        CallWidgetSetFeedRT(RT);
        CameraFeed->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        CameraFeed->SetVisibility(ESlateVisibility::Collapsed);
    }
}

// Bind input for assignment focus (Q = drop)
void AThirdPersonCameraManPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Minimal key binds for assignment focus
    if (InputComponent)
    {
        // Q drops the current active camera (operator only)
        InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AThirdPersonCameraManPlayerController::DropActiveCamera);
    }
}

void AThirdPersonCameraManPlayerController::FeedSetRigOther()
{
    FeedSetRigByIndex(1);
}

// Client drop command: ask server to drop active camera
void AThirdPersonCameraManPlayerController::DropActiveCamera()
{
    if (!IsLocalController()) return;
    Server_DropActiveCamera();
}

// Server drop: stop capture, detach, clear ActiveCamera; viewer will return to pawn
void AThirdPersonCameraManPlayerController::Server_DropActiveCamera_Implementation()
{
    if (!HasAuthority()) return;
    ADirectorGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr;
    if (!GS) return;
    ACameraRig* Rig = GS->ActiveCamera;
    if (!Rig) return;

    // Only operator PC may drop
    if (AThirdPersonCameraManGameMode* GM = Cast<AThirdPersonCameraManGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->GetOperatorPC() != this) return;
    }

    if (Rig->SceneCapture)
    {
        Rig->SceneCapture->bCaptureEveryFrame = false;
    }
    Rig->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (AThirdPersonCameraManGameMode* GM2 = Cast<AThirdPersonCameraManGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GM2->SetActiveCamera(nullptr);
    }
    else
    {
        GS->ActiveCamera = nullptr;
    }

    if (GEngine)
    {
        FString RigName;
#if WITH_EDITOR
        RigName = Rig->GetActorLabel();
#else
        RigName = Rig->GetName();
#endif
        const FString Msg = FString::Printf(TEXT("player dropped :  %s"), *RigName);
        GEngine->AddOnScreenDebugMessage(770779, 2.0f, FColor::Yellow, Msg);
    }
}

void AThirdPersonCameraManPlayerController::UpdateInputMappingsForRole()
{
    if (!IsLocalPlayerController()) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        // If role isn't resolved yet, don't clear anything. This avoids removing
        // template-added contexts at startup which can disable movement.
        if (!bRoleResolved)
        {
            return;
        }

        // Clear any of our known contexts first (safe even if not present)
        for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
        {
            if (CurrentContext) Subsystem->RemoveMappingContext(CurrentContext);
        }
        for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
        {
            if (CurrentContext) Subsystem->RemoveMappingContext(CurrentContext);
        }

        if (bIsOperator)
        {
            for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
            {
                if (CurrentContext) Subsystem->AddMappingContext(CurrentContext, 0);
            }

            if (!SVirtualJoystick::ShouldDisplayTouchInterface())
            {
                for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
                {
                    if (CurrentContext) Subsystem->AddMappingContext(CurrentContext, 0);
                }
            }
        }
    }
}

void AThirdPersonCameraManPlayerController::ShowRoleLabel() const
{
    if (!IsLocalController()) return;
    if (!GEngine) return;

    const FColor Color = bIsOperator ? FColor::Green : FColor::Cyan;

    const UWorld* World = GetWorld();
    const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
    const TCHAR* NetModeStr = (NetMode == NM_ListenServer) ? TEXT("Server")
        : (NetMode == NM_Client) ? TEXT("Client")
        : (NetMode == NM_Standalone) ? TEXT("Standalone")
        : TEXT("Other");

    const FString MyName = PlayerState ? PlayerState->GetPlayerName() : TEXT("None");
    const ADirectorGameState* GS = World ? World->GetGameState<ADirectorGameState>() : nullptr;
    const FString OpName = (GS && GS->OperatorPlayerState) ? GS->OperatorPlayerState->GetPlayerName() : TEXT("None");

    const FString Text = FString::Printf(TEXT("[%s] %s | Me=%s | Operator=%s"),
        NetModeStr,
        bIsOperator ? TEXT("Operator") : TEXT("Viewer"),
        *MyName,
        *OpName);

    // Long duration so it stays visible during testing; reuses same key to update
    GEngine->AddOnScreenDebugMessage(RoleMessageKey, 10000.f, Color, Text);
}

