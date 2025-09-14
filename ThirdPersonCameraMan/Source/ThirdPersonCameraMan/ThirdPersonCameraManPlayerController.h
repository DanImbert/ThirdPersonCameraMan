// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ThirdPersonCameraManPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UCameraFeedWidget;
class UTextureRenderTarget2D;
class ADirectorGameState;
class ACameraRig;

UENUM(BlueprintType)
enum class EFeedCorner : uint8
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS()
class AThirdPersonCameraManPlayerController : public APlayerController
{
    GENERATED_BODY()
    
protected:

	

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	
	// Set this in your BP PlayerController defaults to your BP widget class
	UPROPERTY(EditDefaultsOnly, Category="CameraFeed")
	TSubclassOf<UUserWidget> CameraFeedClass;

    UPROPERTY() UUserWidget* CameraFeed = nullptr;

    UFUNCTION() void HandleActiveCameraChanged(ACameraRig* NewCam);
    void CallWidgetSetFeedRT(UTextureRenderTarget2D* RT);
    UFUNCTION() void HandleOperatorChanged(class APlayerState* NewOperator);

    // Retry hook if ActiveCamera's RenderTarget isn't ready yet on client
    UFUNCTION() void RetryApplyFeedRT();
    FTimerHandle FeedRetryHandle;

    bool IsOperatorCached() const { return bIsOperator; }
    void RefreshLocalRoleFromGameState();
    void UpdateInputMappingsForRole();

    void ShowRoleLabel() const;
    static constexpr int32 RoleMessageKey = 770031; // stable key to update message

    bool bIsOperator = false;
    bool bRoleResolved = false; // true once GameState knows OperatorPlayerState

    // Overlay configuration
    UPROPERTY(EditAnywhere, Category="CameraFeed|Overlay") bool bOverlayForOperator = true;
    UPROPERTY(EditAnywhere, Category="CameraFeed|Overlay") bool bOverlayForViewer = true;
    UPROPERTY(EditAnywhere, Category="CameraFeed|Overlay") int32 OverlayZOrder = 50;
    UPROPERTY(EditAnywhere, Category="CameraFeed|Overlay") FVector2D OverlaySize = FVector2D(512.f, 288.f);
    UPROPERTY(EditAnywhere, Category="CameraFeed|Overlay") FVector2D OverlayMargin = FVector2D(16.f, 16.f);
    UPROPERTY(EditAnywhere, Category="CameraFeed|Overlay") EFeedCorner OverlayCorner = EFeedCorner::BottomRight;

    void EnsureCameraFeedWidget();
    void UpdateFeedOverlayLayout();

public:
    // Force using the active rig as the view target (useful for Simulate/PIE testing)
    UPROPERTY(EditAnywhere, Category="CameraFeed|Debug") bool bForceViewFromActiveRig = false;
    UFUNCTION(Exec) void RigForceView(bool bEnable = true);

    // Local-only override to view a specific render target in the PiP feed
    UPROPERTY(Transient) UTextureRenderTarget2D* FeedOverrideRT = nullptr;
    UFUNCTION(Exec) void FeedSetRigByIndex(int32 Index = 0);
    UFUNCTION(Exec) void FeedClearOverride();
    UFUNCTION(Exec) void FeedToggle();
    void FeedSetRigOther();

    // Drop current active camera (server authoritative). Bound to Q.
    UFUNCTION(BlueprintCallable, Category="Camera") void DropActiveCamera();
    UFUNCTION(Server, Reliable) void Server_DropActiveCamera();
    // Console helpers to calibrate camera rigs at runtime
    UFUNCTION(Exec) void RigCycleAxis();
    UFUNCTION(Exec) void RigNudge(float Yaw = 5.f, float Pitch = 0.f, float Roll = 0.f);
    UFUNCTION(Exec) void RigZeroRoll(bool bZero = true);
    UFUNCTION(Exec) void RigPrint();
	





};
