#include "EnginePCH.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "../Component/ActorComponent.h"
#include "../ObjectSystem/ObjectFactory.h"

// 카메라 컴포넌트를 기본 서브오브젝트로 만들어 루트에 연결한다.
ACameraActor::ACameraActor()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	SetRootComponent(CameraComponent);

	// 레벨에 놓인 카메라가 WASD로 움직이지 않게 한다. 움직임은 소유자(F8 뷰포트, 시퀀서 등)가 직접 처리한다
	CameraComponent->SetExternalInputManaged(true);
}

void ACameraActor::CalcCamera(struct FMinimalViewInfo& OutResult)
{
	if (CameraComponent)
	{
		CameraComponent->GetCameraView(OutResult);
		return;
	}

	Super::CalcCamera(OutResult);
}