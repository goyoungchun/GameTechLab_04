#pragma once
#include "GameFramework/Actor/LightActor.h"


class UBillboardComponent;
class UAmbientLightComponent;

class AAmbientLightActor final : public ALightActor
{
	DECLARE_CLASS(AAmbientLightActor, ALightActor)


public:
	AAmbientLightActor();
	virtual ~AAmbientLightActor() override = default;

	AAmbientLightActor(const AAmbientLightActor&) = delete;
	AAmbientLightActor& operator=(const AAmbientLightActor&) = delete;
private:
	const FString AmbientLightOnIconPath =
		"Assets/Icons/Component/AmbientLightOnfIcon.png";
	const FString AmbientLightOfffIconPath =
		"Assets/Icons/Component/AmbientLightOfffIcon.png";
};