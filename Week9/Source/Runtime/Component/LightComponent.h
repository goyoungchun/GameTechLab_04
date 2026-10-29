#pragma once

#include "SceneComponent.h"
#include "Math/Vector4.h"

enum class ELightType : uint8
{
	Ambient,
	Directional,
	Point,
	Spot
};

enum class EShadowMapResolution : uint32
{
	ShadowMapResolution256 = 256,
	ShadowMapResolution512 = 512,
	ShadowMapResolution1024 = 1024,
	ShadowMapResolution2048 = 2048,
};

class ULightComponent : public USceneComponent
{
	DECLARE_CLASS(ULightComponent, USceneComponent)
	REFLECT_START(ULightComponent)
		PROPERTY_TYPE(LightColor, Color)
		PROPERTY(Intensity)
		PROPERTY(bEnabled)
		PROPERTY(bShowLightBounds)
		PROPERTY(ShadowSourceRadius)
		PROPERTY_ENUM(ShadowMapResolution,
			FEnumEntry(EShadowMapResolution::ShadowMapResolution256, "256"),
			FEnumEntry(EShadowMapResolution::ShadowMapResolution512, "512"),
			FEnumEntry(EShadowMapResolution::ShadowMapResolution1024, "1024"),
			FEnumEntry(EShadowMapResolution::ShadowMapResolution2048, "2048"))
		PROPERTY_RANGE(ShadowBias, 0.0f, 1.0f)
		PROPERTY_RANGE(ShadowSlopeBias, 0.0f, 1.0f)
	REFLECT_END()
public:
	~ULightComponent() override { UnregisterComponent(); }
	virtual ELightType GetLightType() const = 0;
	const FVector& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector& InColor) { LightColor = InColor; OnPropertyChanged("LightColor"); }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = std::max(0.0f, InIntensity); OnPropertyChanged("Intensity"); }
	bool IsEnabled() const { return bEnabled; }
	bool IsLightBoundsVisible() const { return bShowLightBounds; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; OnPropertyChanged("bEnabled"); }
	float GetShadowBias() const { return ShadowBias; }
	void SetShadowBias(float InShadowBias) { ShadowBias = std::clamp(InShadowBias, 0.0f, 1.0f); OnPropertyChanged("ShadowBias"); }
	float GetShadowSlopeBias() const { return ShadowSlopeBias; }
	void SetShadowSlopeBias(float InShadowSlopeBias) { ShadowSlopeBias = std::clamp(InShadowSlopeBias, 0.0f, 1.0f); OnPropertyChanged("ShadowSlopeBias"); }
	float GetShadowSourceRadius() const { return ShadowSourceRadius; }
	void SetShadowSourceRadius(float InShadowSourceRadius) { ShadowSourceRadius = std::max(0.0f, InShadowSourceRadius); OnPropertyChanged("ShadowSourceRadius"); }

	EShadowMapResolution GetShadowMapResolutionSetting() const { return ShadowMapResolution; }
	uint32 GetShadowMapResolution() const { return static_cast<uint32>(ShadowMapResolution); }

	void OnPropertyChanged(const FString& PropertyName)
	{
		Super::OnPropertyChanged(PropertyName);

		if (PropertyName == "Intensity" && Intensity < 0.0f) {Intensity = 0.0f;}
	}


protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;

	FVector LightColor{1.0f, 1.0f, 1.0f};
	float Intensity = 1.0f;
	bool bEnabled = true;
	bool bShowLightBounds = true;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowSourceRadius = 0.12f;
	EShadowMapResolution ShadowMapResolution = EShadowMapResolution::ShadowMapResolution512;
};
