#include "EnginePCH.h"
#include "SpringArmComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void USpringArmComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    UpdateDesiredArmLocation(bDoCollisionTest, bEnableCameraLag, bEnableCameraRotationLag, DeltaTime);
}

FRotator USpringArmComponent::GetDesiredRotation() const
{
    return GetWorldRotation();
}

FRotator USpringArmComponent::GetTargetRotation() const
{
    FRotator DesiredRot = GetDesiredRotation();

    if (bUsePawnControlRotation)
    {
        if (APawn* OwingPawn = Cast<APawn>(GetOwner()))
        {
            if (APlayerController* PC = OwingPawn->GetController())
            {
                const FRotator PawnViewRotation = PC->GetCameraRotation();

                if (bInheritPitch) DesiredRot.Pitch = PawnViewRotation.Pitch;
                if (bInheritYaw) DesiredRot.Yaw = PawnViewRotation.Yaw;
                if (bInheritRoll) DesiredRot.Roll = PawnViewRotation.Roll;
            }
        }
    }
    else
    {
        const FRotator RelRot = GetRelativeRotation();

        if (!bInheritPitch) DesiredRot.Pitch = RelRot.Pitch;
        if (!bInheritYaw) DesiredRot.Yaw = RelRot.Yaw;
        if (!bInheritRoll) DesiredRot.Roll = RelRot.Roll;
    }

    return DesiredRot;
}

// bDoTrace: 스프링 암 충돌 검사 수행 여부
// bDoLocationLag: 위치지연 여부
// bDoRotationLag: 회전지연 여부
void USpringArmComponent::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime)
{
    
    FRotator DesiredRot = GetTargetRotation();
    FVector ArmOrigin = GetWorldLocation() + TargetOffset;
    FVector DesiredLoc = ArmOrigin;

    if (!bLagInitialized)
    {
        PreviousDesiredRot = DesiredRot;
        PreviousDesiredLoc = DesiredLoc;
        bLagInitialized = true;
    }

    // 회전 지연
    if (bDoRotationLag)
    {
        DesiredRot = FMath::QuatInterpolationTo(FQuat(PreviousDesiredRot), FQuat(DesiredRot), DeltaTime, CameraRotationLagSpeed).ToFRotator();
    }

    PreviousDesiredRot = DesiredRot;

    // 위치 지연
    if (bDoLocationLag)
    {
        DesiredLoc = FMath::VectorInterpoationTo(PreviousDesiredLoc, DesiredLoc, DeltaTime, CameraLagSpeed);
    }

    PreviousDesiredLoc = DesiredLoc;

    // 스프링 암 길이 적용해서 카메라 위치 계산
    DesiredLoc -= DesiredRot.Quaternion().GetForwardVector() * TargetArmLength;
    DesiredLoc += FMatrix::MakeRotation(DesiredRot).TransformPosition(SocketOffset);
        
    // 충돌검사
    FVector ResultLoc = DesiredLoc;
    if (bDoTrace && (TargetArmLength != 0.0f))
    {
        const FVector Delta = DesiredLoc - ArmOrigin;

        FHitResult HitResult;
        const FRay Ray(ArmOrigin, Delta.Normalized());

        const float SweepDistance = Delta.Size();
        const float SweepRadius = ProbeSize;

        const bool bBlocked = GetWorld()->SweepSingle(
            Ray,
            SweepDistance,
            SweepRadius,
            HitResult,
            GetOwner()
        );

        if (bBlocked)
        {
            const float SafeDistance = HitResult.bStartPenetrating ? 0.0f : HitResult.Distance;
            ResultLoc = ArmOrigin + Ray.Direction * SafeDistance;
        }
    }

    FVector RelLoc = GetWorldMatrix().Inverse().TransformPosition(ResultLoc);
    FQuat RelQuat = GetWorldRotation().Quaternion().Inverse() * DesiredRot.Quaternion();

    for (USceneComponent* Child : GetAttachChildren())
    {
        if (Child)
        {
            Child->SetRelativeLocation(RelLoc);
            Child->SetRelativeRotation(RelQuat.ToFRotator());
        }
    }
}