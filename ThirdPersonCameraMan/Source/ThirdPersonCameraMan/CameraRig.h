// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraRig.generated.h"

UENUM(BlueprintType)
enum class EAssetForwardAxis : uint8
{
    XPlus,
    XMinus,
    YPlus,
    YMinus,
    ZPlus,
    ZMinus
};


class USphereComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UCameraComponent;
class USceneComponent;
class UStaticMesh;

UCLASS()
class THIRDPERSONCAMERAMAN_API ACameraRig : public AActor
{
    GENERATED_BODY()
    
public:	 
    // Sets default values for this actor's properties
    ACameraRig();

    virtual void OnConstruction(const FTransform& Transform) override;

    // Pivot for camera/capture; keeps Mesh as root for BP compatibility
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") USceneComponent* ViewPivot;

    // Optional separate visual mesh so we can rotate/offset visuals without affecting camera
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") UStaticMeshComponent* VisualMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") UStaticMeshComponent* Mesh;

    // Trigger to detect the operator pawn (auto-pickup)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") USphereComponent* PawnTrigger;

    // Trigger to detect other cameras (switching)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") USphereComponent* CameraSwitchTrigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") USceneCaptureComponent2D* SceneCapture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") UCameraComponent* CameraComponent;
	 

    // Friendly label shown in logs/UI (set per placed instance)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rig")
    FName RigLabel = TEXT("Camera Rig");

    // Let BP read the RT off the rig
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture")
	UTextureRenderTarget2D* RenderTarget;

    // Fallback size used if no RenderTarget is assigned in editor
    UPROPERTY(EditDefaultsOnly, Category="Capture", meta=(ClampMin=16, ClampMax=4096))
    int32 FallbackRTWidth = 1280;

    UPROPERTY(EditDefaultsOnly, Category="Capture", meta=(ClampMin=16, ClampMax=4096))
    int32 FallbackRTHeight = 720;

    // Attachment/mount configuration when the operator picks up the rig
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach")
    FName AttachSocketName = TEXT("head");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach")
    FVector AttachRelativeLocation = FVector(0.f, 0.f, 100.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach")
    FRotator AttachRelativeRotation = FRotator(0.f, 0.f, 0.f);

    // If true, on attach we compute an extra adjustment so the rig faces the pawn's forward
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach")
    bool bAlignWithPawnForwardOnAttach = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach")
    bool bZeroRollOnAttach = true;

    // Component local offsets for view relative to pivot
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="View")
    FVector CameraRelativeLocation = FVector(30.f, 15.f, 10.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="View")
    FRotator CameraRelativeRotation = FRotator(-10.f, 0.f, 0.f);

    

    // Visual mesh offsets that won't affect camera orientation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="View|Visual")
    FVector VisualMeshRelativeLocation = FVector(-10.f, -10.f, -10.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="View|Visual")
    FRotator VisualMeshRelativeRotation = FRotator(0.f, 0.f, 0.f);

    // Auto-rotate the VisualMesh so its asset "forward" axis maps to +X (Unreal forward)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="View|Visual")
    bool bAutoApplyVisualForwardAxis = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="View|Visual")
    EAssetForwardAxis AssetForwardAxis = EAssetForwardAxis::YPlus;

    

    

    // If you need to move the visual mesh, do it in BP for now to avoid breaking existing BPs

    // Extra offsets applied after forward-alignment (degrees)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach", meta=(ClampMin=-180, ClampMax=180))
    float AlignYawOffsetDeg = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach", meta=(ClampMin=-89, ClampMax=89))
    float AlignPitchOffsetDeg = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attach", meta=(ClampMin=-180, ClampMax=180))
    float AlignRollOffsetDeg = 0.f;


protected:
	// Called when the game starts or when spawned
    virtual void BeginPlay() override;

	UFUNCTION()
	void OnPawnBegin(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp,
					 int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);

	UFUNCTION()
	void OnCameraBegin(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp,
					   int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);

	
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetCaptureEnabled(bool bEnable);

	void Server_AttachToPawn(class APawn* PawnOperator);
	bool IsActiveOnServer() const;
	void Server_SwitchTo(class ACameraRig* NewRig);

    // tiny debounce to avoid ping-pong on overlap storms
    float LastSwitchTime = -FLT_MAX;
    UPROPERTY(EditDefaultsOnly, Category="Switch") float SwitchCooldown = 0.25f;

    

	

public:	
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Provide camera data when this actor is a view target
    virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
    UFUNCTION(BlueprintCallable, Category="CameraRig")
    void ReapplyViewAlignment(APawn* ReferencePawn);

    UFUNCTION(BlueprintCallable, Category="CameraRig")
    void ApplyLocalOffsets();

    

    

};
