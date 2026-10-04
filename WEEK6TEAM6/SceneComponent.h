#pragma once

#include "ActorComponent.h"
#include "GraphicsManager.h"
#include "FQuaternion.h"
#include "Vector.h"

class FTransform;

class USceneComponent : public UActorComponent
{
	REFLECT_CLASS(USceneComponent, UActorComponent)
public:
	USceneComponent() = default;
	virtual ~USceneComponent() = default;

	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	void BeginDestroy() override;

	virtual void Serialize(FArchive& Ar) override;
	virtual void Deserialize(FArchive& Ar) override;
	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	void SetupAttachment(USceneComponent* ParentComponent, bool KeepWorldTransform = true);
	void DetachFromParent(bool KeepWorldTransform = true);

	FVector GetRelativeLocation() const;
	void SetRelativeLocation(FVector location);
	void SetWorldLocation(FVector WorldLocation);
	FVector GetWorldLocation();

	FQuaternion GetRelativeRotation() const;
	void SetRelativeRotation(FQuaternion rotation);
	void SetWorldRotation(FQuaternion WorldRotation);
	FQuaternion GetWorldRotation();

	FVector GetRelativeScale3D() const;
	void SetRelativeScale3D(FVector scale);

	const FTransform& GetTransform() const;

	const FMatrix& GetWorldMatrix() const;

	inline bool HasParent() const { return mParentComponent != nullptr; }
	inline USceneComponent* GetParentComponent() const { return mParentComponent; }
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

