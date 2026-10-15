#pragma once

#include "GameFramework/Actor/LightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/PointLightComponent.h"

class APointLightActor : public ALightActor
{
	DECLARE_CLASS(APointLightActor, ALightActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	APointLightActor();
	virtual ~APointLightActor() override = default;

private:
	const FString PointLightOffIconPath =
		"Assets/Icons/Component/PointLightOffIcon.png";
	const FString PointLightOnPath =
		"Assets/Icons/Component/PointLightOnIcon.png";
};
