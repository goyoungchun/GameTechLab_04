#pragma once

#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Render/Material.h"
#include "LightComponent.h"

class FLineBatcher;

class UDirectionalLightComponent : public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(ClassName)
		PROPERTY(Intensity)
		PROPERTY_TYPE(LightColor, Color)
	REFLECT_END()

public:
	UDirectionalLightComponent() = default;
	~UDirectionalLightComponent();

	void InitializeComponent();
	
	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;

	float GetIntensity() const { return Intensity; }
	void SetIntensity(float intensity) { Intensity = intensity; MarkLightDirty(); }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(FVector4 lightcolor) { LightColor = lightcolor; MarkLightDirty(); }

	virtual FLightSceneProxy* CreateSceneProxy() const override;

private:
	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};