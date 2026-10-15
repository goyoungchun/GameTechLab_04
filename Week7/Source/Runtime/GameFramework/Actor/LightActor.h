#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/PointLightComponent.h"

// 에디터에서 아이콘(빌보드)으로 보이고, 선택하면 스포트라이트 원뿔이 라인으로 그려진다.
class ALightActor : public AActor
{
	DECLARE_CLASS(ALightActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	ULightComponent* GetLightComponent() const { return LightComponent; }
	void SetLightComponent(ULightComponent* InLightComponent) { LightComponent = InLightComponent; }

	// 아이콘 텍스처를 Off(기본)/On(선택) 두 장으로 지정하고 빌보드에 Off를 끼운다
	void SetIconTextures(const FString& OffIconPath, const FString& OnIconPath);
	// 에디터 선택 여부에 따라 빌보드 아이콘을 On/Off로 바꾼다
	void SetIconSelected(bool bSelected);

public:
	ALightActor();
	virtual ~ALightActor() override = default;

	// 클릭해서 고를 수 있어야 하므로 프리미티브인 빌보드를 루트로 둔다
	UBillboardComponent* BillboardComponent = nullptr;

	ULightComponent* LightComponent = nullptr;

private:
	UMaterial* OffIconMaterial = nullptr;
	UMaterial* OnIconMaterial = nullptr;
};
