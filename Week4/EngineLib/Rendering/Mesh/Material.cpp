// EngineLib/Rendering/Mesh/Material.cpp

#include "Material.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UMaterial, UObject);
IMPLEMENT_SERIALIZATION(UMaterial, UObject,
	{
		mMaterial = nullptr;
	}
)

void UMaterial::Initialize(FName materialName, std::unique_ptr<FMaterial> inMaterial)
{
	mMaterialName = materialName;
	mMaterial = std::move(inMaterial);
}

