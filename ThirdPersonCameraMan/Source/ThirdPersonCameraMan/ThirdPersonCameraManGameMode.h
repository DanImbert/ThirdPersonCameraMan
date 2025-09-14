// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ThirdPersonCameraManGameMode.generated.h"

class ACameraRig;
UCLASS()
class AThirdPersonCameraManGameMode : public AGameModeBase
{
    GENERATED_BODY()
    
public:

    AThirdPersonCameraManGameMode();
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    UPROPERTY() APlayerController* OperatorPC = nullptr; // first joined

    UFUNCTION(BlueprintCallable) void SetActiveCamera(ACameraRig* NewActive);
    UFUNCTION(BlueprintCallable) void ClearActiveCamera();
    UFUNCTION(BlueprintPure) APlayerController* GetOperatorPC() const { return OperatorPC; }
    
};



