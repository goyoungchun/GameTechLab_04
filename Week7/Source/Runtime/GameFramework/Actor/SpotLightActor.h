#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/SpotLightComponent.h"

class ASpotLightActor : public ALightActor
{
	DECLARE_CLASS(ASpotLightActor, ALightActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ASpotLightActor();
	virtual ~ASpotLightActor() override = default;

private:
	const FString SpotLightOffIconPath =
		"Assets/Icons/Component/SpotLightOfffIcon.png";
	const FString SpotLightOnIconPath =
		"Assets/Icons/Component/SpotLightOnfIcon.png";
};
