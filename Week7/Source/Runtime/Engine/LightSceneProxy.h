#pragma once

class FScene;
class ULightComponent;

// FScene이 프록시를 GPU 버퍼별로 나눌 때 쓴다
enum class ELightType : uint8
{
	Ambient,
	Directional,
	Point,
	Spot,
};

class FLightSceneProxy
{
public:
	FLightSceneProxy(const ULightComponent* Component);
	virtual ~FLightSceneProxy() = default;

	FLightSceneProxy(const FLightSceneProxy&) = delete;
	FLightSceneProxy& operator= (const FLightSceneProxy&) = delete;

	// 컴포넌트 값을 프록시로 다시 복사한다. 생성 시와 dirty 처리 시(FScene) 호출
	virtual void UpdateFromComponent(const ULightComponent* InComponent);

	virtual ELightType GetLightType() const = 0;

	FVector Position;

	// FScene이 관리. 값 읽기용이 아니라 등록 해제할 때 역참조용
	FScene* Scene = nullptr;
	ULightComponent* Component = nullptr;
	int32 PackedIndex = INDEX_NONE;
	bool bQueuedForUpdate = false;
};