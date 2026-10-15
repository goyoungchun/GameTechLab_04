#include "EnginePCH.h"
#include "DirectionalLightActor.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"

ADirectionalLightActor::ADirectionalLightActor()
{
	SetLightComponent(CreateDefaultSubobject<UDirectionalLightComponent>("UDirectionalLightComponent"));
	SetRootComponent(LightComponent);
	SetIconTextures(UnHighLightedDirectionalLightIconPath, HighLightedDirectionalLightOnPath);

	BillboardComponent->SetupAttachment(GetRootComponent());
	//빌보드 컴포넌트가 detail 패널에 안보이게함
	BillboardComponent->SetHiddenInDetails(true);


	DirectionArrow = CreateDefaultSubobject<UStaticMeshComponent>("UStaticMeshComponent");
	DirectionArrow->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Arrow"));
	DirectionArrow->SetupAttachment(GetRootComponent(), EAttachmentRule::SnapToTarget);
	DirectionArrow->SetRelativeScale3D({ 0.2f, 0.2f, 0.5f });
	DirectionArrow->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));


}
