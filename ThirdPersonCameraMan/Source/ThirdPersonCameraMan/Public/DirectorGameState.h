// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DirectorGameState.generated.h"

class FLifetimeProperty;

class UTextureRenderTarget2D;

class APlayerState;

class ACameraRig;
UCLASS()
class THIRDPERSONCAMERAMAN_API ADirectorGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ADirectorGameState();
	
	// Add BlueprintReadOnly so widgets can read it
    UPROPERTY(ReplicatedUsing=OnRep_ActiveCamera, BlueprintReadOnly, Category="Cameras")
    class ACameraRig* ActiveCamera = nullptr;


    UFUNCTION()
    void OnRep_ActiveCamera();


	// Delegate clients can bind to for UI updates
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveCameraChanged, ACameraRig*);
	FOnActiveCameraChanged OnActiveCameraChanged;

    UFUNCTION(BlueprintPure, Category="Cameras")
    UTextureRenderTarget2D* GetActiveCameraRenderTarget() const;

    // Replicated identity of the operator (first player who joined)
    UPROPERTY(ReplicatedUsing=OnRep_Operator, BlueprintReadOnly, Category="Players")
    APlayerState* OperatorPlayerState = nullptr;

    UFUNCTION()
    void OnRep_Operator();

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnOperatorChanged, APlayerState*);
    FOnOperatorChanged OnOperatorChanged;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


    // --- Global switch lock to avoid race/ping-pong on quick overlaps ---
public:
    UFUNCTION(BlueprintPure, Category="Cameras")
    bool IsSwitchLocked() const
    {
        const UWorld* W = GetWorld();
        if (!W) return false;
        return (W->TimeSeconds - LastSwitchStamp) < SwitchLockSeconds;
    }

    void StampSwitch() { LastSwitchStamp = GetWorld() ? GetWorld()->TimeSeconds : LastSwitchStamp; }

protected:
    // Timestamp of last successful attach/switch on the server
    float LastSwitchStamp = -FLT_MAX;
    // Small lockout to prevent rapid double-switching
    UPROPERTY(EditDefaultsOnly, Category="Cameras")
    float SwitchLockSeconds = 0.15f;

};
