#include "SceneComponent.h"

#include <format>

#include "Transform.h"
#include "JsonUtil.h"

void USceneComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UActorComponent::Initialize();

	mRelativeTransform.SetLocation(location);
	mRelativeTransform.SetRotation(rotation);
	mRelativeTransform.SetScale(scale3D);
}

USceneComponent::~USceneComponent()
{
}

void USceneComponent::SerializeClass(json::JSON& outJson) const
{
	UActorComponent::SerializeClass(outJson);
	outJson["Properties"]["mRelativeLocation"] = JsonUtils::ToJson(mRelativeTransform.GetLocation());
	outJson["Properties"]["mRelativeRotation"] = JsonUtils::ToJson(mRelativeTransform.GetRotation());
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
			FRotator RelativeRotation;
			FVector RelativeScale;
			DecomposeMatrix(RelativeMatrix, RelativeLocation, RelativeRotation, RelativeScale);

			mRelativeTransform.SetLocation(RelativeLocation);
			mRelativeTransform.SetRotation(RelativeRotation);
			mRelativeTransform.SetScale(RelativeScale);
		}

		mbWorldMatrixDirty = true;
	}
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

FRotator USceneComponent::GetRelativeRotation() const
{
	return mRelativeTransform.GetRotation();
}

void USceneComponent::SetRelativeRotation(FRotator rotation)
{
	mRelativeTransform.SetRotation(rotation);
	PostWorldMatrixChanged();
}

void USceneComponent::SetWorldRotation(FRotator WorldRotation)
{
	FRotator RelativeRotation = WorldRotation;
	if (mParentComponent)
	{
		FRotator ParentWorldRotation = mParentComponent->GetWorldRotation();
		FMatrix ParentInverseMatrix = FMatrix::Rotate(ParentWorldRotation).Transpose();
		FMatrix LocalMatrix = FMatrix::Rotate(WorldRotation) * ParentInverseMatrix;
		RelativeRotation = ExtractRotationFromMatrix(LocalMatrix);
	}

	SetRelativeRotation(RelativeRotation);
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

FVector USceneComponent::GetWorldLocation()
{
	FMatrix WorldMatrix = GetWorldMatrix();
	return FVector(WorldMatrix.M[3][0], WorldMatrix.M[3][1], WorldMatrix.M[3][2]);
}

FRotator USceneComponent::GetWorldRotation()
{
	FMatrix WorldMatrix = GetWorldMatrix();
	return ExtractRotationFromMatrix(WorldMatrix);
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
		Child->PostWorldMatrixChanged();
	}
}

const FMatrix& USceneComponent::GetWorldMatrix()
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
