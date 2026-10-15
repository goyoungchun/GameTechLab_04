#include "EnginePCH.h"
#include "PointLightActor.h"
#include "Asset/AssetManager.h"


APointLightActor::APointLightActor()
{
	// 아이콘용 메시/머티리얼. Plane은 FVertex 포맷이라 DefaultShader로 그대로 그려진다.
	// 라이트 아이콘 텍스처가 준비되면 Property 창에서 머티리얼 슬롯에 끼우면 된다.
	SetLightComponent(CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent"));
	SetRootComponent(LightComponent);
	SetIconTextures(PointLightOffIconPath, PointLightOnPath);

	BillboardComponent->SetupAttachment(GetRootComponent());


	//빌보드 컴포넌트가 detail 패널에 안보이게함
	BillboardComponent->SetHiddenInDetails(true);
	
	
}
