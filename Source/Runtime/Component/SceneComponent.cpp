#include "EnginePCH.h"
#include "Component/SceneComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Collision/FPhysSceneChaos.h"
#include <cmath>

USceneComponent::~USceneComponent()
{
	TArray<USceneComponent*> Children = AttachChildren;
	AttachChildren.Reset();
	for (USceneComponent* Child : Children)
	{
		Child->AttachParent = nullptr;          
		Child->SetupAttachment(AttachParent);   
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->GetRootComponent() == this)
			OwnerActor->SetRootComponent(Children.Num() > 0 ? Children[0] : nullptr);
	}

	DetachFromParent();
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
	if (InParent == this || AttachParent == InParent) return;

	// 순환 체크
	for (USceneComponent* Parent = InParent; Parent != nullptr; Parent = Parent->AttachParent)
		if (Parent == this) return;

	DetachFromParent();
	AttachParent = InParent;
	if (AttachParent)
	{
		AttachParent->AttachChildren.Add(this);
	}
}

void USceneComponent::DetachFromParent()
{
	if (!AttachParent) return;

	TArray<USceneComponent*>& Siblings = AttachParent->AttachChildren;
	for (uint32 i = 0;i < Siblings.Num(); ++i)
	{
		if (Siblings[i] == this)
		{
			Siblings.RemoveAt(i, 1);
			break;
		}
	}
	AttachParent = nullptr;
}

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetWorldRotation().Quaternion();
		FQuat LocalQuat = Transform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return Transform.Rotation;
}

void USceneComponent::SetWorldLocation(FVector InLocation)
{
	if (AttachParent)
	{
		FMatrix ParentInverseWorldMatrix = AttachParent->GetWorldMatrix().Inverse();
		SetRelativeLocation(ParentInverseWorldMatrix.TransformPosition(InLocation));
	}
	else
	{
		SetRelativeLocation(InLocation);
	}
}

FVector USceneComponent::GetWorldLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
}

FVector USceneComponent::GetWorldScale3D() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetWorldScale3D();

		return FVector(
			Transform.Scale.X * ParentScale.X,
			Transform.Scale.Y * ParentScale.Y,
			Transform.Scale.Z * ParentScale.Z
		);
	}

	return Transform.Scale;
}

FMatrix USceneComponent::GetWorldMatrix() const
{
	FMatrix LocalMatrix = Transform.GetLocalMatrix(); // 부모 컴포넌트 연결 없을 때

	if (AttachParent)
	{
		return LocalMatrix * AttachParent->GetWorldMatrix();
	}

	return LocalMatrix;
}

void USceneComponent::MarkTransformDirty()
{
	OnTransformDirty();         

	for (USceneComponent* Child : AttachChildren)
		Child->MarkTransformDirty();      // 부모가 움직이면 자식의 월드 행렬도 바뀐다
}



void USceneComponent::OnPropertyChanged(const FString& PropertyName)
{
	Super::OnPropertyChanged(PropertyName);
	if (PropertyName == "Transform") MarkTransformDirty();
}
bool USceneComponent::MoveComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit)
{
	return MoveComponentImpl(Delta, NewRotation, bSweep, OutHit);
}

bool USceneComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit)
{
	return MoveComponentImpl(Delta, NewRotation.Quaternion(), bSweep, OutHit);
}

float USceneComponent::GetCollisionRadius() const
{
	// 충돌 반지름은 내접 구체의 반지름으로 설정, Extent의 최소값을 사용한다.
	FBox Bounds = CalcBounds();
	FVector Extent = Bounds.Max - Bounds.Min;
	Extent *= 0.5f;
	// Extent가 0이면 기본값 10.0f 반환
	if(Extent.IsZero())
	{
		return 10.0f;
	}
	// 최소값 반환
	return std::min({ Extent.X, Extent.Y, Extent.Z });
}

bool USceneComponent::MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit)
{
	// OutHit이 nullptr이면 LocalHit을 사용하여 충돌 결과를 저장
    FHitResult LocalHit(1.0f);
    FHitResult& Hit = OutHit ? *OutHit : LocalHit;
    Hit = FHitResult(1.0f);

	// 현재 컴포넌트의 상대 변환을 가져온다.
    FTransform Relative = GetTransform();
    const USceneComponent* Parent = GetAttachParent();
    FMatrix ParentInverse = FMatrix::Identity;
    if (Parent)
    {
        const FMatrix ParentWorld = Parent->GetWorldMatrix();
        const float Determinant = ParentWorld.Determinant();
        if (!std::isfinite(Determinant) || Determinant == 0.0f) return false;
        ParentInverse = ParentWorld.Inverse();
        Relative.Rotation = (Parent->GetWorldRotation().Quaternion().Inverse() * NewRotation).ToFRotator();
    }
    else Relative.Rotation = NewRotation.ToFRotator();

    if (Delta.IsZero())
    {
        if (NewRotation.Equals(GetWorldRotation().Quaternion())) return false;
        SetTransform(Relative);
        return true;
    }

	// 이동량이 0이 아니면 충돌 체크를 수행하고, 충돌이 발생하면 이동량을 조정한다.
    const FVector Start = GetWorldLocation();
    FVector Destination = Start + Delta;
    bool bBlocked = false;
	TArray<FHitResult> SweepOverlaps;
	UPrimitiveComponent* MovingPrimitive = Cast<UPrimitiveComponent>(this);
	UWorld* SweepWorld = nullptr;
	float SweepRadius = 0.0f;
    if (bSweep && (!GetOwner() || GetOwner()->GetActorEnableCollision()))
    {
        AActor* OwnerActor = GetOwner();
        UWorld* World = GetWorld();
        if (!World && OwnerActor) World = OwnerActor->GetWorld();
        if (!World) return false;
		// 이동량의 방향을 정규화하여 Ray를 생성하고, SweepSingle 함수를 호출하여 충돌 체크를 수행한다.
        const FRay Ray(Start, Delta.Normalized());
		SweepWorld = World;
		SweepRadius = GetCollisionRadius();
        bBlocked = World->SweepSingle(Ray, Delta.Size(), SweepRadius, Hit, OwnerActor, MovingPrimitive, &SweepOverlaps);
		// 충돌이 발생하면 이동량을 조정하여 충돌 지점까지 이동하도록 한다.
        if (bBlocked)
		{
			constexpr float ContactOffset = 1e-4f;
			const float PushOut = Hit.bStartPenetrating ? Hit.PenetrationDepth + ContactOffset : std::min(ContactOffset, Hit.Distance);
            Destination = Start + Delta * Hit.Time + Hit.Normal * PushOut;
		}
    }
	// 충돌 체크가 끝나면, 부모 컴포넌트가 있으면 부모의 월드 행렬을 이용하여 상대 위치를 계산하고, 없으면 절대 위치를 사용한다.
    Relative.Location = Parent ? ParentInverse.TransformPosition(Destination) : Destination;
    SetTransform(Relative);
	// 이동 도중 사용자 콜백이 대상을 제거하지 않도록 기존 EndFrame 이벤트 경로에 합친다.
	if (SweepWorld && MovingPrimitive)
	{
		const float TravelDistance = std::max(0.0f, (Destination - Start).Dot(Delta / Delta.Size()));
		for (const FHitResult& Overlap : SweepOverlaps)
			if (Overlap.Distance <= TravelDistance + 1e-5f)
				SweepWorld->GetPhysicsScene().RecordSweepResult(MovingPrimitive, Overlap, SweepRadius);
		if (bBlocked) SweepWorld->GetPhysicsScene().RecordSweepResult(MovingPrimitive, Hit, SweepRadius);
	}
    return !bBlocked;
}

void USceneComponent::SetVisibility(bool bNewVisibility, EVisibilityPropagation Propagation)
{
    const bool bChanged = bVisible != bNewVisibility;
    if (bChanged)
    {
        SetVisibleFlag(bNewVisibility);
        OnVisibilityChanged();
    }
    if (Propagation == EVisibilityPropagation::NoPropagation ||
        (Propagation == EVisibilityPropagation::DirtyOnly && !bChanged)) return;

    TArray<USceneComponent*> Pending;
    for (USceneComponent* Child : AttachChildren) if (Child) Pending.Add(Child);
    while (Pending.Num() > 0)
    {
        USceneComponent* Child = Pending[Pending.Num() - 1];
        Pending.RemoveLast();
        if (Propagation == EVisibilityPropagation::Propagate)
            Child->SetVisibility(bNewVisibility, EVisibilityPropagation::NoPropagation);
        // DirtyOnly preserves the descendant's own visibility flag.
        Child->MarkRenderStateDirty();
        for (USceneComponent* Descendant : Child->GetAttachChildren())
            if (Descendant) Pending.Add(Descendant);
    }
}

void USceneComponent::SetHiddenInGame(bool bNewHidden, EVisibilityPropagation Propagation)
{
    const bool bChanged = bHiddenInGame != bNewHidden;
    if (bChanged)
    {
        bHiddenInGame = bNewHidden;
        OnPropertyChanged("bHiddenInGame");
        OnHiddenInGameChanged();
    }
    if (Propagation == EVisibilityPropagation::NoPropagation ||
        (Propagation == EVisibilityPropagation::DirtyOnly && !bChanged)) return;

    TArray<USceneComponent*> Pending;
    for (USceneComponent* Child : AttachChildren) if (Child) Pending.Add(Child);
    while (Pending.Num() > 0)
    {
        USceneComponent* Child = Pending[Pending.Num() - 1];
        Pending.RemoveLast();
        if (Propagation == EVisibilityPropagation::Propagate)
            Child->SetHiddenInGame(bNewHidden, EVisibilityPropagation::NoPropagation);
        Child->MarkRenderStateDirty();
        for (USceneComponent* Descendant : Child->GetAttachChildren())
            if (Descendant) Pending.Add(Descendant);
    }
}

void USceneComponent::SetVisibleFlag(bool bNewVisible)
{
    if (bVisible == bNewVisible) return;
    bVisible = bNewVisible;
    OnPropertyChanged("bVisible");
}
