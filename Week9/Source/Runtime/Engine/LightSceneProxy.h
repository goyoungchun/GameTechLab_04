#pragma once

#include "Component/LightComponent.h"
#include "Engine/ShadowMapInfo.h"
#include <variant>

class ULightComponent;
class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;

struct FAmbientLightRenderData
{
	FVector Color;
	float Intensity;
};
static_assert(sizeof(FAmbientLightRenderData) % 16 == 0);

struct FDirectionalLightRenderData
{
	FVector Color;
	float Intensity;

	FVector Direction;
	float ShadowAngularRadiusTan;

	float ShadowTexelWorld[8];
	float ShadowNear[8];
	float ShadowFar[8];
	uint32 ShadowMapIndex[8];
	float ShadowSplitDepths[8];
	FMatrix ViewProjection[8];

	uint32 CascadeCount;
	uint32 CascadeExponential;
	float CascadeBlendRange;
	float bCascadeDebug;
};

struct FPointLightRenderData
{
	FVector Color;
	float Intensity;
	FVector Location;
	float AttenuationRadiusSquaredInversed;
	FMatrix ViewProjection[6];
	float ShadowSourceRadius;
	float ShadowNear;
	float ShadowFar;
	float ShadowProjectionScale;
	FVector4 ShadowAtlasRect[6];
}; 

struct FSpotLightRenderData
{
	FVector Color;
	float Intensity;
	FVector Location;
	float AttenuationRadiusSquaredInversed;
	FVector Direction;
	float CosInner;
	float CosOuter;
	FMatrix ViewProjection;
	float ShadowSourceRadius;
	float ShadowNear;
	float ShadowFar;
	float ShadowProjectionScale;
	FVector4 ShadowAtlasRect;
};

inline constexpr uint32 DeferredMaxPointLights = 32;
inline constexpr uint32 InvalidShadowSlot = ~0u;

// 월드의 라이트 컴포넌트를 렌더러가 읽을 수 있는 값으로 복사한다.
class FLightSceneProxy
{
	using FLightRenderData = std::variant<
		FAmbientLightRenderData,
		FDirectionalLightRenderData,
		FPointLightRenderData,
		FSpotLightRenderData>;

public:
	uint32 GetLightId() const { return LightId; }
	explicit FLightSceneProxy(ULightComponent* InComponent);
	void Update();
	// UPointLightComponent* GetComponent() const { return Component; }
	ULightComponent* GetComponent() const { return Component; }
	ELightType GetLightType() const { return LightType; }
	bool IsEnabled() const { return bEnabled; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }
	FLightRenderData& GetRenderData() { return RenderData; }
	const FLightRenderData& GetRenderData() const { return RenderData; }
	const FVector& GetCullingSphereCenter() const { return CullingSphereCenter; }
	float GetCullingSphereRadius() const { return CullingSphereRadius; }
	const FShadowMapInfo& GetShadowMapInfo() const { return ShadowMapInfo; }
	uint32 GetShadowMapResolution() const { return ShadowMapResolution; }
	void SetFirstShadowSlot(uint32 InSlot) { FirstShadowSlot = InSlot; }
	uint32 GetFirstShadowSlot() const { return FirstShadowSlot; }
	bool HasShadowSlot() const { return FirstShadowSlot != InvalidShadowSlot; }

private:
	const uint32 LightId;
	void UpdateSpotSphere(float Radius, float OuterAngle);

	FAmbientLightRenderData MakeAmbientLightData(const UAmbientLightComponent* LightComponent);
	FDirectionalLightRenderData MakeDirectionalLightData(const UDirectionalLightComponent* LightComponent);
	FPointLightRenderData MakePointLightData(const UPointLightComponent* LightComponent);
	FSpotLightRenderData MakeSpotLightData(const USpotLightComponent* LightComponent);

	FVector CullingSphereCenter{};
	float CullingSphereRadius = 0.0f;
	float SpotSphereOffset = 0.0f;
	float CachedSpotRadius = -1.0f;
	float CachedSpotOuterAngle = -1.0f;

	// UPointLightComponent* Component = nullptr;
	ULightComponent* Component = nullptr;
	ELightType LightType = ELightType::Point;
	FLightRenderData RenderData{};
	bool bEnabled = false;
	bool bVisible = true;
	bool bHiddenInGame = false;

	FShadowMapInfo ShadowMapInfo;
	uint32 ShadowMapResolution = 512;
	
	uint32 FirstShadowSlot = InvalidShadowSlot;
};
