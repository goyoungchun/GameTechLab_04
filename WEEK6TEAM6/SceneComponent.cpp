#include "SceneComponent.h"
#include "Transform.h"
#include "JsonUtil.h"
#include "Serializers.h"
#include <format>

void USceneComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UActorComponent::Initialize();

	mRelativeTransform.SetLocation(location);
	mRelativeTransform.SetRotation(rotation);
	mRelativeTransform.SetScale(scale3D);
}

void USceneComponent::BeginDestroy()
{
	while (mChildComponents.Num())
	{
		mChildComponents.Last()->DetachFromParent(true);
	}
	DetachFromParent(false);

	Super::BeginDestroy();
}

void USceneComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << mRelativeTransform;
	Ar << mParentComponent;

	TArray<USceneComponent*> ChildComponents;
	for (USceneComponent* Child : mChildComponents)
	{
		if (!Child->ShouldSerialize())
		{
			continue;
		}

		ChildComponents.Add(Child);
	}

	Ar << ChildComponents;
}

void USceneComponent::Deserialize(FArchive& Ar)
{
	Super::Deserialize(Ar);

	Ar << mRelativeTransform;
	Ar << mParentComponent;
	Ar << mChildComponents;
}

void USceneComponent::SerializeClass(json::JSON& outJson) const
{
	UActorComponent::SerializeClass(outJson);
	outJson["Properties"]["mRelativeLocation"] = JsonUtils::ToJson(mRelativeTransform.GetLocation());
	outJson["Properties"]["mRelativeRotation"] = JsonUtils::ToJson(ToEulerAngles(mRelativeTransform.GetRotation()));
	outJson["Properties"]["mRelativeScale3D"] = JsonUtils::ToJson(mRelativeTransform.GetScale());
}

void USceneComponent::DeserializeClass(const json::JSON& inJson)
{
	UActorComponent::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("mRelativeLocation")
		|| propertiesJson.at("mRelativeLocation").JSONType() != json::JSON::Class::Array
		|| propertiesJson.at("mRelativeLocation").length() != 3)
	{
		throw std::runtime_error(std::format("{}: mRelativeLocation property requires an array of length 3", GetClass()->Name));
	}

	if (!propertiesJson.hasKey("mRelativeRotation")
		|| propertiesJson.at("mRelativeRotation").JSONType() != json::JSON::Class::Array
		|| propertiesJson.at("mRelativeRotation").length() != 3)
	{
		throw std::runtime_error(std::format("{}: mRelativeRotation property requires an array of length 3", GetClass()->Name));
	}

	if (!propertiesJson.hasKey("mRelativeScale3D")
		|| propertiesJson.at("mRelativeScale3D").JSONType() != json::JSON::Class::Array
		|| propertiesJson.at("mRelativeScale3D").length() != 3)
	{
		throw std::runtime_error(std::format("{}: mRelativeScale3D property requires an array of length 3", GetClass()->Name));
	}

	mRelativeTransform.SetLocation(JsonUtils::FromJson<FVector>(propertiesJson.at("mRelativeLocation")));
	mRelativeTransform.SetRotation(JsonUtils::FromJson<FRotator>(propertiesJson.at("mRelativeRotation")));
	mRelativeTransform.SetScale(JsonUtils::FromJson<FVector>(propertiesJson.at("mRelativeScale3D")));
}

void USceneComponent::SetupAttachment(USceneComponent* ParentComponent, bool KeepWorldTransform)
{
	if (!ParentComponent)
	{
		DetachFromParent(KeepWorldTransform);
		return;
	}

	if (mParentComponent == ParentComponent)
	{
		return;
	}

	if (!CanAttachTo(ParentComponent))
	{
		return;
	}

	FMatrix CurrentWorldMatrix = GetWorldMatrix();

	if (mParentComponent)
	{
		int32 Index = mParentComponent->mChildComponents.Find(this);
		if (Index != -1)
		{
			mParentComponent->mChildComponents.RemoveAtSwap(Index);
		}
	}

	mParentComponent = ParentComponent;
	if (mParentComponent)
	{
		mParentComponent->mChildComponents.Add(this);

		if (KeepWorldTransform)
		{
			FMatrix ParentWorldMatrix = mParentComponent->GetWorldMatrix();
			FMatrix ParentInverseMatrix = ParentWorldMatrix.AffineInverse();
			FMatrix RelativeMatrix = CurrentWorldMatrix * ParentInverseMatrix;

			FVector RelativeLocation;
			FQuaternion RelativeRotation;
			FVector RelativeScale;
			DecomposeMatrix(RelativeMatrix, RelativeLocation, RelativeRotation, RelativeScale);

			mRelativeTransform.SetLocation(RelativeLocation);
			mRelativeTransform.SetRotation(RelativeRotation);
			mRelativeTransform.SetScale(RelativeScale);
		}
	}

	PostWorldMatrixChanged();
}

bool USceneComponent::CanAttachTo(USceneComponent* ParentComponent) const
{
	// Prevent circular attachment
	const USceneComponent* CurrentParent = ParentComponent;
	while (CurrentParent)
	{
		if (CurrentParent == this)
		{
			return false;
		}

		CurrentParent = CurrentParent->mParentComponent;
	}

	return true;
}

void USceneComponent::DetachFromParent(bool KeepWorldTransform)
{
	if (!mParentComponent)
	{
		return;
	}
	
	int32 Index = mParentComponent->mChildComponents.Find(this);
	if (Index != -1)
	{
		mParentComponent->mChildComponents.RemoveAtSwap(Index);
	}

	if (KeepWorldTransform)
	{
		FMatrix CurrentWorldMatrix = GetWorldMatrix();

		FVector RelativeLocation;
		FQuaternion RelativeRotation;
		FVector RelativeScale;
		DecomposeMatrix(CurrentWorldMatrix, RelativeLocation, RelativeRotation, RelativeScale);

		mRelativeTransform.SetLocation(RelativeLocation);
		mRelativeTransform.SetRotation(RelativeRotation);
		mRelativeTransform.SetScale(RelativeScale);
	}
	mParentComponent = nullptr;

	PostWorldMatrixChanged();
}

FVector USceneComponent::GetRelativeLocation() const
{
	return mRelativeTransform.GetLocation();
}

void USceneComponent::SetRelativeLocation(FVector location)
{
	mRelativeTransform.SetLocation(location);
	PostWorldMatrixChanged();
}

void USceneComponent::SetWorldLocation(FVector WorldLocation)
{
	FVector RelativeLocation = WorldLocation;
	if (mParentComponent)
	{
		FMatrix ParentWorldMatrix = mParentComponent->GetWorldMatrix();
		FMatrix ParentInverseMatrix = ParentWorldMatrix.AffineInverse();
		RelativeLocation = ParentInverseMatrix.TransformPosition(WorldLocation);
	}

	SetRelativeLocation(RelativeLocation);
}

FVector USceneComponent::GetWorldLocation()
{
	FMatrix WorldMatrix = GetWorldMatrix();
	return FVector(WorldMatrix.M[3][0], WorldMatrix.M[3][1], WorldMatrix.M[3][2]);
}

FQuaternion USceneComponent::GetRelativeRotation() const
{
	return mRelativeTransform.GetRotation();
}

void USceneComponent::SetRelativeRotation(FQuaternion rotation)
{
	mRelativeTransform.SetRotation(rotation);
	PostWorldMatrixChanged();
}

void USceneComponent::SetWorldRotation(FQuaternion WorldRotation)
{
	FQuaternion RelativeRotation = WorldRotation;
	if (mParentComponent)
	{
		FQuaternion ParentWorldRotation = mParentComponent->GetWorldRotation();
		FQuaternion ParentInverseRotation = ParentWorldRotation.Inverse();
		RelativeRotation = ParentInverseRotation * WorldRotation;
		RelativeRotation.Normalize();
	}

	SetRelativeRotation(RelativeRotation);
}

FQuaternion USceneComponent::GetWorldRotation()
{
	FQuaternion Rotation = GetRelativeRotation();

	if (mParentComponent)
	{
		Rotation = mParentComponent->GetWorldRotation() * Rotation;
	}
	Rotation.Normalize();

	return Rotation;
}

FVector USceneComponent::GetRelativeScale3D() const
{
	return mRelativeTransform.GetScale();
}

void USceneComponent::SetRelativeScale3D(FVector scale)
{
	mRelativeTransform.SetScale(scale);
	PostWorldMatrixChanged();
}

const FTransform& USceneComponent::GetTransform() const
{
	return mRelativeTransform;
}

void USceneComponent::PostWorldMatrixChanged()
{
	mbWorldMatrixDirty = true;
	OnTransformChanged();

	for (USceneComponent* Child : mChildComponents)
	{
		if (Child->mbWorldMatrixDirty)
		{
			continue;
		}

		Child->PostWorldMatrixChanged();
	}
}

const FMatrix& USceneComponent::GetWorldMatrix() const
{
	if (mbWorldMatrixDirty)
	{
		mWorldMatrix = mRelativeTransform.MakeMatrix();
		if (mParentComponent)
		{
			mWorldMatrix *= mParentComponent->GetWorldMatrix();
		}
		
		mbWorldMatrixDirty = false;
	}

	return mWorldMatrix;
}
