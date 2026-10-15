#include "EnginePCH.h"
#include "LightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"

ALightActor::ALightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
}

// 아이콘 머티리얼을 복사해 텍스처만 바꾼 인스턴스를 만든다. 텍스처를 못 읽으면 nullptr
static UMaterial* CreateIconMaterial(const FString& IconPath)
{
	UTexture2D* IconTexture = UAssetManager::Get().LoadTexture(IconPath);
	if (!IconTexture)
		return nullptr;

	UMaterial* IconMaterial = UMaterial::CreateInstance(UAssetManager::GetAssetByPath<UMaterial>("BillboardIconMaterial"));
	if (IconMaterial)
	{
		IconMaterial->Textures.Reset();
		IconMaterial->Textures.Add(IconTexture);
	}
	return IconMaterial;
}

void ALightActor::SetIconTextures(const FString& OffIconPath, const FString& OnIconPath)
{
	OffIconMaterial = CreateIconMaterial(OffIconPath);
	OnIconMaterial = CreateIconMaterial(OnIconPath);

	SetIconSelected(false);
}

void ALightActor::SetIconSelected(bool bSelected)
{
	// 아이콘을 지정하지 않은 라이트는 기본 머티리얼을 그대로 둔다
	if (UMaterial* IconMaterial = bSelected ? OnIconMaterial : OffIconMaterial)
		BillboardComponent->SetMaterial(0, IconMaterial);
}
