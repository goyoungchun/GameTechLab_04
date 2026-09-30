#include "SphereComponent.h"

#include "Rendering/RenderInfo.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(USphereComponent, UStaticMeshComponent);

IMPLEMENT_SERIALIZATION(USphereComponent, UStaticMeshComponent, {})

USphereComponent::USphereComponent()
{
}

/*
void USphereComponent::Initialize(GraphicsManager* graphicsManager)
{
	Initialize(graphicsManager, FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void USphereComponent::Initialize(GraphicsManager* graphicsManager, FVector location, FRotator rotation, FVector scale3D)
{
	UPrimitiveComponent::Initialize(graphicsManager, EPrimitive::EP_Sphere, location, rotation, scale3D);
}
*/

USphereComponent::~USphereComponent()
{
}

void USphereComponent::Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos)
{
	if (mbSpin)
	{
		mElapsedDegrees += mSpinSpeed * deltaTime;
		if (mElapsedDegrees >= 360.f)
		{
			mElapsedDegrees = std::fmod(mElapsedDegrees, 360.f); // Wrap around to keep the angle in [0, 360)
		}

		// Update sub uv instead of the rotation of the sphere component
		mSubUVMesh.UVOffset.x = -mElapsedDegrees / 360.f; // Assuming the texture is a horizontal strip of frames
	}

	UStaticMeshComponent::Update(deltaTime, outRenderInfos);
}

void USphereComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f), nullptr, false, 90.f);
}

void USphereComponent::Initialize(FVector location, FRotator rotation, FVector scale3D,
	const UStaticMesh* sphereMeshOrNull,
	bool bSpin, float spinSpeed)
{
	UStaticMeshComponent::Initialize(location, rotation, scale3D, sphereMeshOrNull, true);
	mbSpin = bSpin;
	mSpinSpeed = spinSpeed;
}

FRenderInfo USphereComponent::makeRenderInfo() const
{
	FRenderInfo renderInfo = UStaticMeshComponent::makeRenderInfo();

	return renderInfo;
}

std::span<const FPropertyInfo>
USphereComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			USphereComponent,
			mbSpin,
			EPropertyFlags::Serializable | EPropertyFlags::Editable),
		REFLECT_PROPERTY(
			USphereComponent,
			mSpinSpeed,
			EPropertyFlags::Serializable | EPropertyFlags::Editable)
	};

	return Properties;
}
