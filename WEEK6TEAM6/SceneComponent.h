#pragma once

#include "ActorComponent.h"
#include "GraphicsManager.h"

#include "Vector.h"

class FTransform;

class USceneComponent : public UActorComponent
{
	REFLECT_CLASS(USceneComponent, UActorComponent)
public:
	USceneComponent() = default;
	virtual ~USceneComponent();

	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	void SetupAttachment(USceneComponent* ParentComponent, bool KeepWorldTransform = true);

	FVector GetRelativeLocation() const;
	void SetRelativeLocation(FVector location);
	void SetWorldLocation(FVector WorldLocation);

	FRotator GetRelativeRotation() const;
	void SetRelativeRotation(FRotator rotation);
	void SetWorldRotation(FRotator WorldRotation);

	FVector GetRelativeScale3D() const;
	void SetRelativeScale3D(FVector scale);

	FVector GetWorldLocation();
	FRotator GetWorldRotation();

	const FTransform& GetTransform() const;

	const FMatrix& GetWorldMatrix();

	inline bool HasParent() const { return mParentComponent != nullptr; }
	inline const TArray<USceneComponent*>& GetChildComponents() const { return mChildComponents; }

protected:
	virtual void OnTransformChanged() {}

private:
	bool CanAttachTo(USceneComponent* ParentComponent) const;

	void PostWorldMatrixChanged();

private:
	FTransform mRelativeTransform;

	mutable bool mbWorldMatrixDirty = true;
	mutable FMatrix mWorldMatrix = FMatrix::Identity;

	USceneComponent* mParentComponent = nullptr;
	TArray<USceneComponent*> mChildComponents;
};

