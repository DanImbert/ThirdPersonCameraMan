// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ThirdPersonCameraManCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS()
class AThirdPersonCameraManCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

    /** Constructor */
    AThirdPersonCameraManCharacter(); 

public:
    // When a CameraRig attaches to this character, it can pull these settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Attach")
    FName AttachCameraRigSocket = TEXT("head");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Attach")
    FVector AttachCameraRigRelativeLocation = FVector(0.f, 0.f, 100.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Attach")
    FRotator AttachCameraRigRelativeRotation = FRotator(0.f, 0.f, 0.f);

    // Per-character desired camera offsets on the rig's camera component
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig View")
    FVector RigCameraRelativeLocation = FVector(30.f, 15.f, 10.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig View")
    FRotator RigCameraRelativeRotation = FRotator(-10.f, 0.f, 0.f);

    // Alignment behavior for the rig when attached to this character
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Align")
    bool bRigAlignWithPawnForwardOnAttach = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Align")
    bool bRigZeroRollOnAttach = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Align", meta=(ClampMin=-180, ClampMax=180))
    float RigAlignYawOffsetDeg = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Align", meta=(ClampMin=-89, ClampMax=89))
    float RigAlignPitchOffsetDeg = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CameraRig Align", meta=(ClampMin=-180, ClampMax=180))
    float RigAlignRollOffsetDeg = 0.f;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

