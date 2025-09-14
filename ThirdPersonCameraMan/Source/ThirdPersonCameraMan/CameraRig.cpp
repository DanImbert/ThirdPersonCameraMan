#include "CameraRig.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Camera/CameraComponent.h"

#include "ThirdPersonCameraManGameMode.h"
#include "DirectorGameState.h"
#include "ThirdPersonCameraManCharacter.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

#include <cfloat> // for FLT_MAX

DEFINE_LOG_CATEGORY_STATIC(LogDirectorRig, Log, All);

ACameraRig::ACameraRig()
{

    
    bReplicates = true;
    SetReplicateMovement(true);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Optional visual-only mesh; lets us offset/rotate visuals without changing camera basis
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(RootComponent);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetGenerateOverlapEvents(false);
    VisualMesh->bReceivesDecals = false;

    // Trigger for pawn pickup
    PawnTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PawnTrigger"));
    PawnTrigger->SetupAttachment(RootComponent);
    PawnTrigger->InitSphereRadius(120.f);
    PawnTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PawnTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    PawnTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PawnTrigger->SetGenerateOverlapEvents(true);
    PawnTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACameraRig::OnPawnBegin);

    // Trigger for camera-to-camera switching
    CameraSwitchTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("CameraSwitchTrigger"));
    CameraSwitchTrigger->SetupAttachment(RootComponent);
    CameraSwitchTrigger->InitSphereRadius(150.f);
    CameraSwitchTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CameraSwitchTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    CameraSwitchTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    CameraSwitchTrigger->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
    CameraSwitchTrigger->SetGenerateOverlapEvents(true);
    CameraSwitchTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACameraRig::OnCameraBegin);

    ViewPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewPivot"));
    ViewPivot->SetupAttachment(RootComponent);

    SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
    SceneCapture->SetupAttachment(ViewPivot);
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;

    // Local camera so viewers can set this rig as their view target
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(ViewPivot);

    LastSwitchTime = -FLT_MAX;
}

FString ACameraRig::GetRigDisplayName() const
{
    if (RigLabel != NAME_None)
    {
        return RigLabel.ToString();
    }
#if WITH_EDITOR
    return GetActorLabel();
#else
    return GetName();
#endif
}

void ACameraRig::BeginPlay()
{
    Super::BeginPlay();
    // Ensure a valid render target exists, even if not set in editor
    if (!RenderTarget)
    {
        // Create a transient RT so all clients can capture and display
        RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
            this,
            FallbackRTWidth > 0 ? FallbackRTWidth : 1280,
            FallbackRTHeight > 0 ? FallbackRTHeight : 720,
            ETextureRenderTargetFormat::RTF_RGBA8
        );
    }
    else if (!RenderTarget->HasAnyFlags(RF_Transient))
    {
        // The assigned RT is likely a shared asset. Create a unique transient copy per rig
        const int32 Width = (RenderTarget->SizeX > 0 ? RenderTarget->SizeX : FallbackRTWidth);
        const int32 Height = (RenderTarget->SizeY > 0 ? RenderTarget->SizeY : FallbackRTHeight);
        UTextureRenderTarget2D* UniqueRT = UKismetRenderingLibrary::CreateRenderTarget2D(
            this,
            (Width > 0 ? Width : 1280),
            (Height > 0 ? Height : 720),
            ETextureRenderTargetFormat::RTF_RGBA8
        );
        if (UniqueRT)
        {
            RenderTarget = UniqueRT;
        }
    }

    ApplyLocalOffsets();

    // If VisualMesh has no asset, mirror Mesh's asset into it and hide the root mesh
    if (VisualMesh && Mesh)
    {
        if (!VisualMesh->GetStaticMesh() && Mesh->GetStaticMesh())
        {
            VisualMesh->SetStaticMesh(Mesh->GetStaticMesh());
            Mesh->SetHiddenInGame(true);
            Mesh->SetVisibility(false, true);
        }

        // Apply current offsets once at begin play
        ApplyLocalOffsets();
    }
}



bool ACameraRig::IsActiveOnServer() const
{
    if (!HasAuthority()) return false;
    if (const ADirectorGameState* GS = Cast<ADirectorGameState>(GetWorld()->GetGameState()))
    {
        return GS->ActiveCamera == this;
    }
    return false;
}

void ACameraRig::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    // Compute a first-person style view from our attachment reference, zeroing roll
    FTransform RefXf = GetActorTransform();
    if (USceneComponent* Parent = (GetRootComponent() ? GetRootComponent()->GetAttachParent() : nullptr))
    {
        if (const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Parent))
        {
            const FName Sock = GetRootComponent()->GetAttachSocketName();
            if (!Sock.IsNone() && Skel->DoesSocketExist(Sock))
            {
                RefXf = Skel->GetSocketTransform(Sock, RTS_World);
            }
            else
            {
                RefXf = Skel->GetComponentTransform();
            }
        }
        else
        {
            RefXf = Parent->GetComponentTransform();
        }
    }

    // Derive forward from the chosen asset axis to handle meshes authored along Y/Z
    FVector AxisFwd(1,0,0);
    switch (AssetForwardAxis)
    {
    case EAssetForwardAxis::XPlus:  AxisFwd = FVector( 1, 0, 0); break;
    case EAssetForwardAxis::XMinus: AxisFwd = FVector(-1, 0, 0); break;
    case EAssetForwardAxis::YPlus:  AxisFwd = FVector( 0, 1, 0); break;
    case EAssetForwardAxis::YMinus: AxisFwd = FVector( 0,-1, 0); break;
    case EAssetForwardAxis::ZPlus:  AxisFwd = FVector( 0, 0, 1); break;
    case EAssetForwardAxis::ZMinus: AxisFwd = FVector( 0, 0,-1); break;
    default: break;
    }

    const FVector Fwd = RefXf.GetRotation().RotateVector(AxisFwd).GetSafeNormal();
    const FRotator NoRoll = FRotationMatrix::MakeFromXZ(Fwd, FVector::UpVector).Rotator();

    FRotator CamRot = NoRoll;
    CamRot.Yaw   += AlignYawOffsetDeg;
    CamRot.Pitch += AlignPitchOffsetDeg;
    if (!bZeroRollOnAttach)
    {
        CamRot.Roll += AlignRollOffsetDeg;
    }

    const FVector CamLoc = RefXf.GetLocation() + CamRot.RotateVector(CameraRelativeLocation);
    OutResult.Location = CamLoc;
    OutResult.Rotation = CamRot;
    OutResult.FOV = CameraComponent ? CameraComponent->FieldOfView : 90.f;
}

// Local offsets: place camera (lens) and visual mesh (prop body) independently
void ACameraRig::ApplyLocalOffsets()
{
    if (SceneCapture)
    {
        SceneCapture->SetRelativeLocation(CameraRelativeLocation);
        SceneCapture->SetRelativeRotation(CameraRelativeRotation);
    }
    if (CameraComponent)
    {
        CameraComponent->SetRelativeLocation(CameraRelativeLocation);
        CameraComponent->SetRelativeRotation(CameraRelativeRotation);
    }
    if (VisualMesh)
    {
        VisualMesh->SetRelativeLocation(VisualMeshRelativeLocation);

        FRotator FinalVisualRot = VisualMeshRelativeRotation;
        if (bAutoApplyVisualForwardAxis)
        {
            FVector Src = FVector::ForwardVector;
            switch (AssetForwardAxis)
            {
            case EAssetForwardAxis::XPlus:  Src = FVector(1,0,0); break;
            case EAssetForwardAxis::XMinus: Src = FVector(-1,0,0); break;
            case EAssetForwardAxis::YPlus:  Src = FVector(0,1,0); break;
            case EAssetForwardAxis::YMinus: Src = FVector(0,-1,0); break;
            case EAssetForwardAxis::ZPlus:  Src = FVector(0,0,1); break;
            case EAssetForwardAxis::ZMinus: Src = FVector(0,0,-1); break;
            default: break;
            }
            const FQuat Delta = FQuat::FindBetweenVectors(Src.GetSafeNormal(), FVector::ForwardVector);
            const FRotator AxisAdjust = Delta.Rotator();
            FinalVisualRot = (FinalVisualRot + AxisAdjust).GetNormalized();
        }
        VisualMesh->SetRelativeRotation(FinalVisualRot);
    }
}

// Upright align: face pawn forward; apply yaw/pitch/roll offsets
void ACameraRig::ReapplyViewAlignment(APawn* ReferencePawn)
{
    if (!ReferencePawn)
    {
        if (AThirdPersonCameraManGameMode* GM = Cast<AThirdPersonCameraManGameMode>(GetWorld()->GetAuthGameMode()))
        {
            if (APlayerController* PC = GM->GetOperatorPC())
            {
                ReferencePawn = PC->GetPawn();
            }
        }
    }

    // Apply local offsets first to ensure pivot/camera are in expected pose
    ApplyLocalOffsets();

    if (!ReferencePawn)
    {
        return;
    }

    if (bAlignWithPawnForwardOnAttach)
    {
        const FVector PawnFwd = ReferencePawn->GetActorForwardVector().GetSafeNormal();
        const FRotator DesiredNoRoll = FRotationMatrix::MakeFromXZ(PawnFwd, FVector::UpVector).Rotator();

        FRotator NewWorld = DesiredNoRoll;
        NewWorld.Yaw   += AlignYawOffsetDeg;
        NewWorld.Pitch += AlignPitchOffsetDeg;
        if (!bZeroRollOnAttach)
        {
            NewWorld.Roll += AlignRollOffsetDeg;
        }

        // Rotate the actor so that ViewPivot (and thus camera) faces forward
        SetActorRotation(NewWorld);
    }
}

void ACameraRig::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // no per-tick logic; we switch on overlaps
}

void ACameraRig::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyLocalOffsets();
}

// Pickup handler: gate to operator, then call server attach
void ACameraRig::OnPawnBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority()) return;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) { UE_LOG(LogDirectorRig, Verbose, TEXT("[Rig %s] Overlap ignored: not a pawn"), *GetName()); return; }

    if (AThirdPersonCameraManGameMode* GM = Cast<AThirdPersonCameraManGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (APlayerController* Operator = GM->GetOperatorPC())
        {
            if (Operator == Pawn->GetController())
            {
                UE_LOG(LogDirectorRig, Log, TEXT("[Rig %s] Pickup requested by %s"), *GetName(), *Operator->GetName());
                Server_AttachToPawn(Pawn);
            }
            else
            {
                UE_LOG(LogDirectorRig, Verbose, TEXT("[Rig %s] Overlap ignored: controller %s is not operator"), *GetName(), *GetNameSafe(Pawn->GetController()));
            }
        }
        else
        {
            UE_LOG(LogDirectorRig, Warning, TEXT("[Rig %s] Overlap ignored: OperatorPC not set in GameMode"), *GetName());
        }
    }
}

// Switch handler: only when this rig is active; switch to other rig
void ACameraRig::OnCameraBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority()) return;

    const float Now = GetWorld()->TimeSeconds;
    if (Now - LastSwitchTime < SwitchCooldown) return;

    if (!IsActiveOnServer()) return;

    ACameraRig* OtherRig = Cast<ACameraRig>(OtherActor);
    if (!OtherRig || OtherRig == this)
    {
        UE_LOG(LogDirectorRig, Verbose, TEXT("[Rig %s] Switch overlap ignored: Other=%s"), *GetName(), *GetNameSafe(OtherActor));
        return;
    }

    LastSwitchTime = Now;
    UE_LOG(LogDirectorRig, Log, TEXT("[Rig %s] Switching to nearby rig %s"), *GetName(), *OtherRig->GetName());
    Server_SwitchTo(OtherRig);
}

// Server-authoritative pickup: attach to pawn, align, enable capture, set ActiveCamera
void ACameraRig::Server_AttachToPawn(APawn* PawnOperator)
{
    if (!HasAuthority() || !PawnOperator) return;

    // Global switch lock to avoid double processing
    if (ADirectorGameState* LGS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr)
    {
        if (LGS->IsSwitchLocked())
        {
            UE_LOG(LogDirectorRig, Verbose, TEXT("[Rig %s] Attach ignored due to switch lock"), *GetName());
            return;
        }
        LGS->StampSwitch();
    }

    if (ADirectorGameState* GS = Cast<ADirectorGameState>(GetWorld()->GetGameState()))
    {
        // Already active? No-op
        if (GS->ActiveCamera == this)
        {
            UE_LOG(LogDirectorRig, Verbose, TEXT("[Rig %s] Already ActiveCamera"), *GetName());
            return;
        }
        if (ACameraRig* Old = GS->ActiveCamera)
        {
            if (Old != this)
            {
                Old->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                if (Old->SceneCapture) Old->SceneCapture->bCaptureEveryFrame = false;
            }

            Old->Multicast_SetCaptureEnabled(false);
        }
    }

    USceneComponent* AttachTarget = PawnOperator->GetRootComponent();
    FName SocketToUse = NAME_None;

    // Defaults from rig
    FName DesiredSocket = AttachSocketName;
    FVector DesiredRelLoc = AttachRelativeLocation;
    FRotator DesiredRelRot = AttachRelativeRotation;

    // If the pawn is our character class, override from character-configured values
    if (AThirdPersonCameraManCharacter* TPChar = Cast<AThirdPersonCameraManCharacter>(PawnOperator))
    {
        if (!TPChar->AttachCameraRigSocket.IsNone())
        {
            DesiredSocket = TPChar->AttachCameraRigSocket;
        }
        DesiredRelLoc = TPChar->AttachCameraRigRelativeLocation;
        DesiredRelRot = TPChar->AttachCameraRigRelativeRotation;

        // Also adopt the per-character desired camera offsets for this rig
        CameraRelativeLocation = TPChar->RigCameraRelativeLocation;
        CameraRelativeRotation = TPChar->RigCameraRelativeRotation;

        // And adopt per-character alignment behavior
        bAlignWithPawnForwardOnAttach = TPChar->bRigAlignWithPawnForwardOnAttach;
        bZeroRollOnAttach            = TPChar->bRigZeroRollOnAttach;
        AlignYawOffsetDeg            = TPChar->RigAlignYawOffsetDeg;
        AlignPitchOffsetDeg          = TPChar->RigAlignPitchOffsetDeg;
        AlignRollOffsetDeg           = TPChar->RigAlignRollOffsetDeg;
    }

    if (ACharacter* Char = Cast<ACharacter>(PawnOperator))
    {
        if (USkeletalMeshComponent* MeshComp = Char->GetMesh())
        {
            // Prefer skeletal mesh so we can use a socket like "head"
            AttachTarget = MeshComp;
            if (!DesiredSocket.IsNone() && MeshComp->DoesSocketExist(DesiredSocket))
            {
                SocketToUse = DesiredSocket;
            }
        }
    }

    AttachToComponent(AttachTarget,
                      FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                      SocketToUse);

    // Apply configurable relative offset so the camera isn't at the feet/floor
    SetActorRelativeLocation(DesiredRelLoc);
    SetActorRelativeRotation(DesiredRelRot);

    // Immediately realign upright to pawn forward (zero roll if requested)
    if (bAlignWithPawnForwardOnAttach)
    {
        ReapplyViewAlignment(PawnOperator);
    }

    // Reapply offsets now that we've snapped to pawn
    ApplyLocalOffsets();

    if (SceneCapture && RenderTarget)
    {
        SceneCapture->TextureTarget = RenderTarget;
        SceneCapture->bCaptureEveryFrame = true;
        SceneCapture->CaptureScene();
        UE_LOG(LogDirectorRig, Log, TEXT("[Rig %s] Capture ON RT=%s Size=(%d x %d)"), *GetName(), *GetNameSafe(RenderTarget), RenderTarget->SizeX, RenderTarget->SizeY);
    }

    Multicast_SetCaptureEnabled(true);

    if (AThirdPersonCameraManGameMode* GM = Cast<AThirdPersonCameraManGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GM->SetActiveCamera(this);
        UE_LOG(LogDirectorRig, Log, TEXT("[Rig %s] ActiveCamera set by server"), *GetName());
    }

    // Announce pickup to all clients in a friendly format: "player N picked up :  <RigName>"
    if (GEngine)
    {
        int32 PlayerNum = 1;
        if (APlayerState* PS = PawnOperator->GetPlayerState())
        {
            if (AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
            {
                TArray<APlayerState*> Sorted = GS->PlayerArray;
                Sorted.Sort([](const APlayerState& L, const APlayerState& R)
                {
                    return L.GetPlayerId() < R.GetPlayerId();
                });
                for (int32 i = 0; i < Sorted.Num(); ++i)
                {
                    if (Sorted[i] == PS)
                    {
                        PlayerNum = i + 1; // 1-based for presentation
                        break;
                    }
                }
            }
        }

        const FString RigName = GetRigDisplayName();
        const FString Msg = FString::Printf(TEXT("player %d picked up :  %s"), PlayerNum, *RigName);
        GEngine->AddOnScreenDebugMessage(770777, 3.f, FColor::Green, Msg);
    }
}

// Server switch: resolve operator pawn; reuse attach; lock prevents ping-pong
void ACameraRig::Server_SwitchTo(ACameraRig* NewRig)
{
    if (!HasAuthority() || !NewRig) return;

    if (ADirectorGameState* LGS = GetWorld() ? GetWorld()->GetGameState<ADirectorGameState>() : nullptr)
    {
        if (LGS->IsSwitchLocked())
        {
            UE_LOG(LogDirectorRig, Verbose, TEXT("[Rig %s] Switch ignored due to lock"), *GetName());
            return;
        }
        LGS->StampSwitch();
    }

    APawn* OperatorPawn = nullptr;
    if (AThirdPersonCameraManGameMode* GM = Cast<AThirdPersonCameraManGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (APlayerController* Operator = GM->GetOperatorPC())
        {
            OperatorPawn = Operator->GetPawn();
        }
    }
    if (!OperatorPawn)
    {
        UE_LOG(LogDirectorRig, Warning, TEXT("[Rig %s] Switch failed: no operator pawn"), *GetName());
        return;
    }

    if (SceneCapture) SceneCapture->bCaptureEveryFrame = false;

    Multicast_SetCaptureEnabled(false);

    UE_LOG(LogDirectorRig, Log, TEXT("[Rig %s] Server switching operator to %s"), *GetName(), *NewRig->GetName());
    NewRig->Server_AttachToPawn(OperatorPawn);
}

// CameraRig.cpp
void ACameraRig::Multicast_SetCaptureEnabled_Implementation(bool bEnable)
{
    if (SceneCapture && RenderTarget)
    {
        SceneCapture->TextureTarget = RenderTarget;
        SceneCapture->bCaptureEveryFrame = bEnable;
        if (bEnable)
        {
            SceneCapture->CaptureScene();
        }
    }
}



