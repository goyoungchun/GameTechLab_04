#include "EnginePCH.h"
#include "CameraModifier_CameraShake.h"
#include "CameraShakeBase.h"
#include "ObjectSystem/ObjectFactory.h"

UCameraModifier_CameraShake::UCameraModifier_CameraShake()
{
	Priority = 0;
}

UCameraModifier_CameraShake::~UCameraModifier_CameraShake()
{
	StopAllCameraShakes();
}

void UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	for (int32 i = ActiveShakes.Num() - 1; i >= 0; --i)
	{
		if (!ActiveShakes[i]->UpdateAndApply(DeltaTime, InOutPOV, GetAlpha()))
		{
			delete ActiveShakes[i];
			ActiveShakes.RemoveAt(i, 1);
		}
	}
}

void UCameraModifier_CameraShake::AddCameraShake(const FCameraShakeParams& Params)
{
	if (!Params.IsValid() || Params.Duration <= 0.0f)
	{
		return;
	}

	UCameraShakeBase* Shake = FObjectFactory::ConstructObject<UCameraShakeBase>();
	Shake->Start(Params);
	ActiveShakes.Add(Shake);
}

void UCameraModifier_CameraShake::StopAllCameraShakes()
{
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		delete Shake;
	}

	ActiveShakes.Reset();
}


