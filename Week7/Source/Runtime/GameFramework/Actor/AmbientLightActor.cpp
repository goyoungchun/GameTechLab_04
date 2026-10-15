#include "EnginePCH.h"
#include "AmbientLightActor.h"
#include "Component/AmbientLightComponent.h"
#include "Component/BillboardComponent.h"


AAmbientLightActor::AAmbientLightActor()
{
	SetLightComponent(CreateDefaultSubobject<UAmbientLightComponent>("UAmbientLightComponent"));
	SetRootComponent(LightComponent);

	SetIconTextures(AmbientLightOfffIconPath, AmbientLightOnIconPath);
	BillboardComponent->SetupAttachment(GetRootComponent());
	//빌보드 컴포넌트가 detail 패널에 안보이게함
	BillboardComponent->SetHiddenInDetails(true);


}
