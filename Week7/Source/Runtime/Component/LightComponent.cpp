#include "EnginePCH.h"
#include "Component/LightComponent.h"
#include "Engine/Scene.h"

void ULightComponent::MarkLightDirty()
{
	if (SceneProxy && SceneProxy->Scene)
		SceneProxy->Scene->MarkLightDirty(SceneProxy);
}

void ULightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);   // Transform 변경은 부모가 OnTransformDirty로 넘겨준다

	// 에디터에서 리플렉션으로 값을 직접 바꾸면 setter를 거치지 않으므로 여기서 알린다
	MarkLightDirty();
}

void ULightComponent::OnTransformDirty()
{
	MarkLightDirty();
}
