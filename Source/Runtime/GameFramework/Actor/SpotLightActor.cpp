#include "EnginePCH.h"
#include "GameFramework/Actor/SpotLightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Texture/Texture2D.h"

ASpotLightActor::ASpotLightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
	if (UTexture2D* LightIcon = UAssetManager::Get().LoadTexture("Editor/Icon/SpotLight.png"))
	{
		BillboardComponent->SetSprite(LightIcon);
	}
	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>("USpotLightComponent");
	SpotLightComponent->SetupAttachment(BillboardComponent);
}
