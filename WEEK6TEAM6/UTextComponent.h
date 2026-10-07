#pragma once

#include "SceneComponent.h"
#include "PrimitiveComponent.h"
#include "Assets.h"
#include "Camera.h"
#include "Actor.h"
#include "FAssetManager.h"
#include "ShowFlags.h"
#include "MathUtility.h"
#include "Json/json.hpp"
#include "JsonUtil.h"
#include "FTextBuilder.h"
#include "FQuaternion.h"
#include "FArchive.h"
#include "Serializers.h"
#include "FEngine.h"
#include "FFogProcess.h"
#include "Vector.h"

class UPlaneComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UPlaneComponent, UPrimitiveComponent)

public:
	UPlaneComponent()
	{
        SetTickable(true);
		mMeshAsset = FAssetManager::Get().GetAssetAs<FStaticMeshAsset>(FName("PlaneMesh"), true);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		FGuid MeshAssetID = mMeshAsset ? mMeshAsset->GetAssetID() : FGuid();
		Ar << MeshAssetID;

		FGuid TextureAssetID = mTextureAsset ? mTextureAsset->GetAssetID() : FGuid();
		Ar << TextureAssetID;

		Ar << mSubUV;
		Ar << mSubUVOffset;
		Ar << mBlendMode;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);

		FGuid MeshAssetID;
		Ar << MeshAssetID;
		mMeshAsset = FAssetManager::Get().GetAssetAs<FStaticMeshAsset>(MeshAssetID, true);

		FGuid TextureAssetID;
		Ar << TextureAssetID;
		mTextureAsset = FAssetManager::Get().GetAssetAs<FTexture2DAsset>(TextureAssetID, true);

		Ar << mSubUV;
		Ar << mSubUVOffset;
		Ar << mBlendMode;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	void SerializeClass(json::JSON& outJson) const override
	{
		UPrimitiveComponent::SerializeClass(outJson);

		if (mTextureAsset)
		{
			FGuid AssetID = mTextureAsset->GetAssetID();
			outJson["Properties"]["ObjTextureAsset"] = JsonUtils::ToJson(AssetID);
		}
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		UPrimitiveComponent::DeserializeClass(inJson);

		const json::JSON& PropertiesJson = inJson.at("Properties");
		if (!PropertiesJson.hasKey("ObjTextureAsset"))
		{
			throw std::runtime_error("UPlaneComponent: ObjTextureAsset property is required");
		}

		if (PropertiesJson.at("ObjTextureAsset").JSONType() != json::JSON::Class::Object)
		{
			throw std::runtime_error("UPlaneComponent: ObjTextureAsset property requires an object");
		}

		FGuid AssetID = JsonUtils::FromJson<FGuid>(PropertiesJson.at("ObjTextureAsset"));
		if (AssetID.IsValid())
		{
			mTextureAsset = FAssetManager::Get().GetAssetAs<FTexture2DAsset>(AssetID, true);
		}
	}

	void Render(FRenderCollector& RenderCollector) override
	{
		Super::Render(RenderCollector);

		mRenderProxy->SetCollector(RenderCollector);
		mRenderProxy->ReserveRenderTransparentQuadInfos(1);

		FMatrix PivotMatrix = GetWorldMatrix();

		FRenderQuadInfo& QuadInfo = mRenderProxy->GetRenderTransparentQuadInfo(0);
		QuadInfo.Model = PivotMatrix;
		QuadInfo.Color = FVector4(1.f, 1.f, 1.f, 1.f);
		QuadInfo.TextureSRV = mTextureAsset ? mTextureAsset->GetSRV() : nullptr;
		QuadInfo.TextureFormat = mTextureAsset ? mTextureAsset->GetFormat() : DXGI_FORMAT_UNKNOWN;
		QuadInfo.SubUV = mSubUV + FVector4(mSubUVOffset.X, mSubUVOffset.Y, 0.f, 0.f);
		QuadInfo.BlendMode = mBlendMode;
		QuadInfo.EnableDepthTest = mEnableDepthTest;
		QuadInfo.EnableDepthWrite = mEnableDepthWrite;

		mRenderProxy->SetActiveRenderTransparentQuadInfoNum(1);
	}

	FAABB GetBoundingBox() override
	{
		if (!mMeshAsset)
		{
			return FAABB();
		}

		const FMatrix& WorldMatrix = GetWorldMatrix();
		return mMeshAsset->GetLocalBoundingBox().ToWorld(WorldMatrix);
	}

	const TArray<FVertex>& GetMeshVertices() const override
	{
		if (!mMeshAsset)
		{
			return UPrimitiveComponent::GetMeshVertices();
		}
		return mMeshAsset->GetVertices();
	}

	const TArray<uint32>& GetMeshIndices() const override
	{
		if (!mMeshAsset)
		{
			return UPrimitiveComponent::GetMeshIndices();
		}

		return mMeshAsset->GetIndices();
	}

	inline void SetTexture(const TSharedPtr<FTexture2DAsset>& textureAsset) 
	{ 
		mTextureAsset = textureAsset; 
		MarkRenderDirty();
	}

	inline const TSharedPtr<FTexture2DAsset>& GetTexture() const { return mTextureAsset; }

	inline void SetDepthState(bool enableDepthTest, bool enableDepthWrite)
	{ 
		mEnableDepthTest = enableDepthTest; 
		mEnableDepthWrite = enableDepthWrite; 
		MarkRenderDirty();
	}

	inline void SetBlendState(ERenderBlendMode InBlendMode) 
	{ 
		mBlendMode = InBlendMode; 
		MarkRenderDirty();
	}

protected:
	TSharedPtr<FStaticMeshAsset> mMeshAsset;
	TSharedPtr<FTexture2DAsset> mTextureAsset;
	FVector4 mSubUV = { 0.f, 0.f, 1.f, 1.f };
	FVector2 mSubUVOffset = { 0.f, 0.f };

	ERenderBlendMode mBlendMode = ERenderBlendMode::Opaque;
	bool mEnableDepthTest = true;
	bool mEnableDepthWrite = true;
};

class UBillboardComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UBillboardComponent, UPrimitiveComponent)

public:
	UBillboardComponent()
	{
		SetTickable(true);
		bTickInEditor = true;

		mMeshAsset = FAssetManager::Get().GetAssetAs<FStaticMeshAsset>(FName("PlaneMesh"), true);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		FGuid MeshAssetID = mMeshAsset ? mMeshAsset->GetAssetID() : FGuid();
		Ar << MeshAssetID;

		FGuid TextureAssetID = mTextureAsset ? mTextureAsset->GetAssetID() : FGuid();
		Ar << TextureAssetID;

		Ar << mSubUV;
		Ar << mSubUVOffset;
		Ar << mBlendMode;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);

		FGuid MeshAssetID;
		Ar << MeshAssetID;
		mMeshAsset = FAssetManager::Get().GetAssetAs<FStaticMeshAsset>(MeshAssetID, true);

		FGuid TextureAssetID;
		Ar << TextureAssetID;
		mTextureAsset = FAssetManager::Get().GetAssetAs<FTexture2DAsset>(TextureAssetID, true);

		Ar << mSubUV;
		Ar << mSubUVOffset;
		Ar << mBlendMode;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	void SerializeClass(json::JSON& outJson) const override
	{
		UPrimitiveComponent::SerializeClass(outJson);

		if (mTextureAsset)
		{
			FGuid AssetID = mTextureAsset->GetAssetID();
			outJson["Properties"]["ObjTextureAsset"] = JsonUtils::ToJson(AssetID);
		}
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		UPrimitiveComponent::DeserializeClass(inJson);

		const json::JSON& PropertiesJson = inJson.at("Properties");
		if (!PropertiesJson.hasKey("ObjTextureAsset"))
		{
			throw std::runtime_error("UBillboardComponent: ObjTextureAsset property is required");
		}

		if (PropertiesJson.at("ObjTextureAsset").JSONType() != json::JSON::Class::Object)
		{
			throw std::runtime_error("UBillboardComponent: ObjTextureAsset property requires an object");
		}

		FGuid AssetID = JsonUtils::FromJson<FGuid>(PropertiesJson.at("ObjTextureAsset"));
		if (AssetID.IsValid())
		{
			mTextureAsset = FAssetManager::Get().GetAssetAs<FTexture2DAsset>(AssetID, true);
		}
	}

	void Tick(float DeltaTime) override
	{
		MarkRenderDirty();
	}

	void Render(FRenderCollector& RenderCollector) override
	{
		Super::Render(RenderCollector);

		mRenderProxy->SetCollector(RenderCollector);
		mRenderProxy->ReserveRenderTransparentQuadInfos(1);

		FMatrix PivotMatrix = GetWorldMatrix();

		FMatrix TranslationMatrix = FMatrix::ExtractTranslation(PivotMatrix);
		FVector Translation = FMatrix::GetTranslation(PivotMatrix);
		FQuaternion BillboardRotation = RenderCollector.Camera->Transform.GetRotation();

		PivotMatrix = ToMatrix(BillboardRotation) * FMatrix::Translation(Translation);

		FRenderQuadInfo& QuadInfo = mRenderProxy->GetRenderTransparentQuadInfo(0);
		QuadInfo.Model = PivotMatrix;
		QuadInfo.Color = FVector4(1.f, 1.f, 1.f, 1.f);
		QuadInfo.TextureSRV = mTextureAsset ? mTextureAsset->GetSRV() : nullptr;
		QuadInfo.TextureFormat = mTextureAsset ? mTextureAsset->GetFormat() : DXGI_FORMAT_UNKNOWN;
		QuadInfo.SubUV = mSubUV + FVector4(mSubUVOffset.X, mSubUVOffset.Y, 0.f, 0.f);
		QuadInfo.BlendMode = mBlendMode;
		QuadInfo.EnableDepthTest = mEnableDepthTest;
		QuadInfo.EnableDepthWrite = mEnableDepthWrite;

		mRenderProxy->SetActiveRenderTransparentQuadInfoNum(1);
	}

	FAABB GetBoundingBox() override
	{
		if (!mMeshAsset)
		{
			return FAABB();
		}

		const FMatrix& WorldMatrix = GetWorldMatrix();
		return mMeshAsset->GetLocalBoundingBox().ToWorld(WorldMatrix);
	}

	const TArray<FVertex>& GetMeshVertices() const override
	{
		if (!mMeshAsset)
		{
			return UPrimitiveComponent::GetMeshVertices();
		}
		return mMeshAsset->GetVertices();
	}

	const TArray<uint32>& GetMeshIndices() const override
	{
		if (!mMeshAsset)
		{
			return UPrimitiveComponent::GetMeshIndices();
		}

		return mMeshAsset->GetIndices();
	}

	inline void SetTexture(const TSharedPtr<FTexture2DAsset>& textureAsset) { mTextureAsset = textureAsset; }
	inline const TSharedPtr<FTexture2DAsset>& GetTexture() const { return mTextureAsset; }

	inline void SetDepthState(bool enableDepthTest, bool enableDepthWrite) { mEnableDepthTest = enableDepthTest; mEnableDepthWrite = enableDepthWrite; }
	void SetBlendState(ERenderBlendMode InBlendMode) { mBlendMode = InBlendMode; }

protected:
	TSharedPtr<FStaticMeshAsset> mMeshAsset;
	TSharedPtr<FTexture2DAsset> mTextureAsset;
	FVector4 mSubUV = { 0.f, 0.f, 1.f, 1.f };
	FVector2 mSubUVOffset = { 0.f, 0.f };

	ERenderBlendMode mBlendMode = ERenderBlendMode::Opaque;
	bool mEnableDepthTest = true;
	bool mEnableDepthWrite = true;
};

class USpotLightComponent : public USceneComponent
{
	REFLECT_CLASS(USpotLightComponent, USceneComponent)

public:
    USpotLightComponent() { SetTickable(true); }

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		Ar << Range;
		Ar << mInnerConeAngle;
		Ar << mOuterConeAngle;
		Ar << mColor;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);

		Ar << Range;
		Ar << mInnerConeAngle;
		Ar << mOuterConeAngle;
		Ar << mColor;
	}

	inline float GetRange() const { return Range; }
	inline float GetInnerConeAngle() const { return mInnerConeAngle; }
	inline float GetOuterConeAngle() const { return mOuterConeAngle; }
	inline const FVector4& GetColor() const { return mColor; }

	inline void SetColor(const FVector4& InColor) { mColor = InColor; }

	inline void SetOuterConeAngle(float InAngle)
	{
		mOuterConeAngle = FMath::Clamp(InAngle, 0.f, MAX_CONE_ANGLE);
		mInnerConeAngle = FMath::Min(mInnerConeAngle, mOuterConeAngle);
	}

	inline void SetInnerConeAngle(float InAngle)
	{
		mInnerConeAngle = FMath::Clamp(InAngle, 0.f, mOuterConeAngle);
	}

private:
	static constexpr float MAX_CONE_ANGLE = 89.f;

	float Range = 5.0f;
	FVector4 mColor = { 1.f, 1.f, 1.f, 1.f };
	float mInnerConeAngle = 30.0f;
	float mOuterConeAngle = 45.0f;
};

class UText3DComponent : public USceneComponent
{
	REFLECT_CLASS(UText3DComponent, USceneComponent)

public:
	UText3DComponent()
	{
		SetRenderable(true);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		Ar << mbBillboard;
		Ar << mText;
		
		FGuid FontAtlasAssetID = mFontAtlasAsset ? mFontAtlasAsset->GetAssetID() : FGuid();
		Ar << FontAtlasAssetID;

		Ar << mColor;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);
		
		Ar << mbBillboard;
		Ar << mText;

		FGuid FontAtlasAssetID;
		Ar << FontAtlasAssetID;

		mFontAtlasAsset = FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FontAtlasAssetID, true);

		Ar << mColor;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	void SerializeClass(json::JSON& outJson) const override
	{
		Super::SerializeClass(outJson);

		// std::wstring을 UTF-8 문자열로 변환하여 저장
		outJson["Properties"]["mText"] = Wide2Utf(mText).CStr();
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		const json::JSON& propertiesJson = inJson.at("Properties");

		// 이전 버전 씬 파일과의 호환성을 위해 필수가 아닌 값으로 처리
		if (propertiesJson.hasKey("mText") && propertiesJson.at("mText").JSONType() == json::JSON::Class::String)
		{
			mText = Utf2Wide(FString(propertiesJson.at("mText").ToString()));
		}
		else
		{
			mText.clear();
		}
	}

	void Render(FRenderCollector& RenderCollector) override
	{
		Super::Render(RenderCollector);

		mRenderProxy->SetCollector(RenderCollector);
		mRenderProxy->SetActiveRenderOverlayQuadInfoNum(0);

		// Show Flags에서 끄면 쿼드를 아예 만들지 않는다.
		// 만들고 거르는 게 아니라 글자 수만큼의 계산 자체가 사라진다.
		if (!FShowFlags::Get().IsEnabled(EShowFlag::UUIDText))
		{
			return;
		}

        if (!mOwner || !mOwner->GetRootComponent()) return;

		if (!mFontAtlasAsset)
		{
			return;
		}

		const TSharedPtr<FFontAtlas>& FontAtlas = mFontAtlasAsset->GetFontAtlas();
		if (!FontAtlas || mText.empty())
		{
			return;
		}

		mRenderProxy->ReserveRenderOverlayQuadInfos(mText.length());

		FTextBuilder TextBuilder(FontAtlas, WorldUnitPerPixel);

		float TotalWidth = 0.0f;
		float TotalHeight = 0.0f;
		TextBuilder.CalculateSize(mText, TotalWidth, TotalHeight);

		FMatrix PivotMatrix = GetWorldMatrix();

		if (mbBillboard && RenderCollector.Camera)
		{
			FMatrix TranslationMatrix = FMatrix::ExtractTranslation(PivotMatrix);
			FVector Translation = FMatrix::GetTranslation(PivotMatrix);
			FQuaternion BillboardRotation = RenderCollector.Camera->Transform.GetRotation();

			UPrimitiveComponent* Primitive = HasParent() ? GetParentComponent()->Cast<UPrimitiveComponent>() : nullptr;
			if (Primitive)
			{
				const FAABB Bounds = Primitive->GetBoundingBox();
				Translation = FVector(Translation.x, Translation.y, Bounds.Max.z + 0.2f);
			}

			PivotMatrix = ToMatrix(BillboardRotation) * FMatrix::Translation(Translation);
		}

		int32 GlyphCount = 0;
		TextBuilder.Build(mText, TotalWidth, TotalHeight, [&](const FRect& Rect, const FRect& UV) {
			// 공백 등은 Builder에서 advance만 적용하고, 쿼드는 생략한다.
			if (Rect.Width <= 0.f || Rect.Height <= 0.f)
			{
				return;
			}

			const FVector GlyphCenter(0.f, Rect.X, Rect.Y);

			FRenderQuadInfo& QuadInfo = mRenderProxy->GetRenderOverlayQuadInfo(GlyphCount);
			QuadInfo.Model = FMatrix::Scale(FVector3(1.f, Rect.Width, Rect.Height)) * FMatrix::Translation(GlyphCenter) * PivotMatrix;
			QuadInfo.Color = mColor;
			QuadInfo.TextureSRV = mFontAtlasAsset->GetSRV();
			QuadInfo.TextureFormat = mFontAtlasAsset->GetFormat();
			QuadInfo.SubUV = FVector4(UV.X, UV.Y, UV.Width, UV.Height);
			QuadInfo.BlendMode = ERenderBlendMode::Transparent;
			QuadInfo.EnableDepthTest = mEnableDepthTest;
			QuadInfo.EnableDepthWrite = mEnableDepthWrite;

			GlyphCount++;
		});

		mRenderProxy->SetActiveRenderOverlayQuadInfoNum(GlyphCount);
	}

	inline void SetBillboard(bool billboard) { mbBillboard = billboard; }

	inline void SetText(const std::wstring& text) { mText = text; }
	inline const std::wstring& GetText() const { return mText; }

	inline void SetFontAtlasAsset(const TSharedPtr<FFontAtlasAsset>& fontAtlasAsset) { mFontAtlasAsset = fontAtlasAsset; }

	inline void SetColor(const FVector4& color) { mColor = color; }
	inline void SetDepthState(bool enableDepthTest, bool enableDepthWrite) { mEnableDepthTest = enableDepthTest; mEnableDepthWrite = enableDepthWrite; }

private:
	bool mbBillboard = false;
	std::wstring mText;
	TSharedPtr<FFontAtlasAsset> mFontAtlasAsset;
	FVector4 mColor = FVector4(1, 1, 1, 1);
	bool mEnableDepthTest = true;
	bool mEnableDepthWrite = true;
};

class ASpotLight : public AActor
{
	REFLECT_CLASS(ASpotLight, AActor)

public:
	ASpotLight()
	{
		USpotLightComponent* SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>(FName("SpotLightComponent"));
		SetRootComponent(SpotLightComponent);
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		for (UActorComponent* Component : GetComponents())
		{
			UBillboardComponent* BillboardComponent = Component->Cast<UBillboardComponent>();

			if (!BillboardComponent)
			{
				continue;
			}

			BillboardComponent->SetBlendState(ERenderBlendMode::Transparent);
			BillboardComponent->SetDepthState(true, false);
		}
	}

	void CreateEditorComponents() override
	{
		UBillboardComponent* BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(FName("SpotLightIcon"));
		BillboardComponent->SetTexture(FAssetManager::Get().GetAssetAs<FTexture2DAsset>(BuiltInAssetID::SpotLightIcon, true));
		BillboardComponent->SetBlendState(ERenderBlendMode::Transparent);
		BillboardComponent->SetDepthState(true, false);
		BillboardComponent->SetEditorOnly(true);
		BillboardComponent->SetDoNotSerialize(true);
		BillboardComponent->SetVisualizeProxy(true);

		USceneComponent* RootComp = GetRootComponent();
		if (RootComp)
		{
			BillboardComponent->SetupAttachment(RootComp);
		}

		AddOwnedComponent(BillboardComponent);

		UText3DComponent* Text3DComponent = CreateDefaultSubobject<UText3DComponent>(FName("UUIDDisplayer"));
		Text3DComponent->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f));
		Text3DComponent->SetBillboard(true);
		Text3DComponent->SetText(Utf2Wide(std::format("UUID: {}", UUID)));
		Text3DComponent->SetFontAtlasAsset(FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FName("TestFontAtlas")));
		Text3DComponent->SetDepthState(false, false);
		Text3DComponent->SetEditorOnly(true);
		Text3DComponent->SetDoNotSerialize(true);

		Text3DComponent->SetupAttachment(BillboardComponent, false);

		AddOwnedComponent(Text3DComponent);
	}
};

class UHeightFogComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UHeightFogComponent, UPrimitiveComponent)

public:
	UHeightFogComponent()
	{
		FogProcess = &GEngine->GetGraphicsManager().GetFogProcess();
		FogComponentCount++;
		FogProcess->SetEnabled(FogComponentCount > 0);
		FogProcess->RegisterFogComponent();
	}

	~UHeightFogComponent()
	{
		FogComponentCount--;
		FogProcess->SetEnabled(FogComponentCount > 0);
		FogProcess->UnregisterFogComponent();
	}

	inline void SetFogDensity(float Density) { FogProcess->FogConstants.FogDensity = Density; }
	inline float GetFogDensity() const { return FogProcess->FogConstants.FogDensity; }

	inline void SetFogHeightFalloff(float Falloff) { FogProcess->FogConstants.FogHeightFalloff = Falloff; }
	inline float GetFogHeightFalloff() const { return FogProcess->FogConstants.FogHeightFalloff; }

	inline void SetFogStartDistance(float StartDistance) { FogProcess->FogConstants.StartDistance = StartDistance; }
	inline float GetFogStartDistance() const { return FogProcess->FogConstants.StartDistance; }

	inline void SetFogCutoffDistance(float CutoffDistance) { FogProcess->FogConstants.FogCutoffDistance = CutoffDistance; }
	inline float GetFogCutoffDistance() const { return FogProcess->FogConstants.FogCutoffDistance; }

	inline void SetFogMaxOpacity(float MaxOpacity) { FogProcess->FogConstants.FogMaxOpacity = MaxOpacity; }
	inline float GetFogMaxOpacity() const { return FogProcess->FogConstants.FogMaxOpacity; }

	inline void SetFogHeight(float Height) { FogProcess->FogConstants.FogHeight = Height; }
	inline float GetFogHeight() const { return FogProcess->FogConstants.FogHeight; }

	inline void SetFogInscatteringColor(const FLinearColor& Color) { FogProcess->FogConstants.FogInscatteringColor = Color; }
	inline FLinearColor GetFogInscatteringColor() const { return FogProcess->FogConstants.FogInscatteringColor; }

private:
	inline static int32 FogComponentCount = 0;

	FFogProcess* FogProcess;
};

class AHeightFog : public AActor
{
	REFLECT_CLASS(AHeightFog, AActor)

public:
	AHeightFog()
	{
		UHeightFogComponent* HeightFogComponent = CreateDefaultSubobject<UHeightFogComponent>(FName("HeightFogComponent"));
		SetRootComponent(HeightFogComponent);
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		for (UActorComponent* Component : GetComponents())
		{
			UBillboardComponent* BillboardComponent = Component->Cast<UBillboardComponent>();

			if (!BillboardComponent)
			{
				continue;
			}

			BillboardComponent->SetBlendState(ERenderBlendMode::Transparent);
			BillboardComponent->SetDepthState(true, false);
		}
	}

	void CreateEditorComponents() override
	{
		UBillboardComponent* BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(FName("HeightFogIcon"));
		BillboardComponent->SetTexture(FAssetManager::Get().GetAssetAs<FTexture2DAsset>(BuiltInAssetID::HeightFogIcon, true));
		BillboardComponent->SetBlendState(ERenderBlendMode::Transparent);
		BillboardComponent->SetDepthState(true, false);
		BillboardComponent->SetEditorOnly(true);
		BillboardComponent->SetDoNotSerialize(true);
		BillboardComponent->SetVisualizeProxy(true);

		USceneComponent* RootComp = GetRootComponent();
		if (RootComp)
		{
			BillboardComponent->SetupAttachment(RootComp);
		}

		AddOwnedComponent(BillboardComponent);

		UText3DComponent* Text3DComponent = CreateDefaultSubobject<UText3DComponent>(FName("UUIDDisplayer"));
		Text3DComponent->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f));
		Text3DComponent->SetBillboard(true);
		Text3DComponent->SetText(Utf2Wide(std::format("UUID: {}", UUID)));
		Text3DComponent->SetFontAtlasAsset(FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FName("TestFontAtlas")));
		Text3DComponent->SetDepthState(false, false);
		Text3DComponent->SetEditorOnly(true);
		Text3DComponent->SetDoNotSerialize(true);

		Text3DComponent->SetupAttachment(BillboardComponent, false);

		AddOwnedComponent(Text3DComponent);
	}
};

class UPointLightComponent : public USceneComponent
{
	REFLECT_CLASS(UPointLightComponent, USceneComponent)

public:
	UPointLightComponent() = default;

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);
		
		Ar << Intensity;
		Ar << Radius;
		Ar << RadiusFallOff;
		Ar << Color;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);

		Ar << Intensity;
		Ar << Radius;
		Ar << RadiusFallOff;
		Ar << Color;
	}

	inline void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	inline float GetIntensity() const { return Intensity; }

	inline void SetRadius(float InRadius) { Radius = InRadius; }
	inline float GetRadius() const { return Radius; }

	inline void SetRadiusFallOff(float InRadiusFallOff) { RadiusFallOff = InRadiusFallOff; }
	inline float GetRadiusFallOff() const { return RadiusFallOff; }

	inline void SetColor(const FLinearColor& InColor) { Color = InColor; }
	inline FLinearColor GetColor() const { return Color; }

private:
	float Intensity = 3.f;
	float Radius = 5.f;
	float RadiusFallOff = 1.f;
	FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 1.f);
};

class UProjectileMovementComponent : public UActorComponent
{
	REFLECT_CLASS(UProjectileMovementComponent, UActorComponent)

public:
	UProjectileMovementComponent()
	{
		SetTickable(true);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);
		Ar << Velocity;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);
		Ar << Velocity;
	}

	void Tick(float DeltaTime) override
	{
		if (!mOwner)
		{
			return;
		}

		USceneComponent* RootComp = mOwner->GetRootComponent();
		if (!RootComp)
		{
			return;
		}

		FVector CurrentLocation = RootComp->GetWorldLocation();
		FVector NewLocation = CurrentLocation + Velocity * DeltaTime;
		RootComp->SetWorldLocation(NewLocation);
	}

	inline void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
	inline FVector GetVelocity() const { return Velocity; }

private:
	FVector Velocity = FVector(1.f, 0.f, 0.f);
};

class URotationMovementComponent : public UActorComponent
{
	REFLECT_CLASS(URotationMovementComponent, UActorComponent)

public:
	URotationMovementComponent()
	{
		SetTickable(true);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);
		
		Ar << RotationAxis;
		Ar << RotationSpeed;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);

		Ar << RotationAxis;
		Ar << RotationSpeed;
	}

	void Tick(float DeltaTime) override
	{
		if (!mOwner)
		{
			return;
		}

		USceneComponent* RootComp = mOwner->GetRootComponent();
		if (!RootComp)
		{
			return;
		}

		FQuaternion CurrentRotation = RootComp->GetWorldRotation();
		FQuaternion DeltaRotation = FQuaternion(RotationAxis, RotationSpeed * DeltaTime);
		FQuaternion NewRotation = DeltaRotation * CurrentRotation;
		NewRotation.Normalize();
		RootComp->SetWorldRotation(NewRotation);
	}

	inline void SetRotationAxis(const FVector& InRotationAxis)
	{
		RotationAxis = InRotationAxis;
		RotationAxis.Normalize();
	}

	inline FVector GetRotationAxis() const { return RotationAxis; }

	inline void SetRotationSpeed(float InRotationSpeed) { RotationSpeed = InRotationSpeed; }
	inline float GetRotationSpeed() const { return RotationSpeed; }

private:
	FVector RotationAxis = FVector(0.f, 0.f, 1.f);
	float RotationSpeed = 1.f;
};

class UTextRenderComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UTextRenderComponent, UPrimitiveComponent)

public:
	UTextRenderComponent()
	{
		SetRenderable(true);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		Ar << mText;

		FGuid FontAtlasAssetID = mFontAtlasAsset ? mFontAtlasAsset->GetAssetID() : FGuid();
		Ar << FontAtlasAssetID;

		Ar << mColor;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	virtual void Deserialize(FArchive& Ar) override
	{
		Super::Deserialize(Ar);

		Ar << mText;

		FGuid FontAtlasAssetID;
		Ar << FontAtlasAssetID;

		mFontAtlasAsset = FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FontAtlasAssetID, true);

		Ar << mColor;
		Ar << mEnableDepthTest;
		Ar << mEnableDepthWrite;
	}

	void Render(FRenderCollector& RenderCollector) override
	{
		Super::Render(RenderCollector);

		mRenderProxy->SetCollector(RenderCollector);
		mRenderProxy->SetActiveRenderTransparentQuadInfoNum(0);

		if (!mOwner || !mOwner->GetRootComponent())
		{
			return;
		}

		if (!mFontAtlasAsset)
		{
			return;
		}

		const TSharedPtr<FFontAtlas>& FontAtlas = mFontAtlasAsset->GetFontAtlas();
		if (!FontAtlas || mText.empty())
		{
			return;
		}

		mRenderProxy->ReserveRenderTransparentQuadInfos(mText.length());

		FTextBuilder TextBuilder(FontAtlas, WorldUnitPerPixel);

		float TotalWidth = 0.0f;
		float TotalHeight = 0.0f;
		TextBuilder.CalculateSize(mText, TotalWidth, TotalHeight);

		FMatrix PivotMatrix = GetWorldMatrix();

		int32 GlyphCount = 0;
		TextBuilder.Build(mText, TotalWidth, TotalHeight, [&](const FRect& Rect, const FRect& UV) {
			// 공백 등은 Builder에서 advance만 적용하고, 쿼드는 생략한다.
			if (Rect.Width <= 0.f || Rect.Height <= 0.f)
			{
				return;
			}

			const FVector GlyphCenter(0.f, Rect.X, Rect.Y);

			FRenderQuadInfo& QuadInfo = mRenderProxy->GetRenderTransparentQuadInfo(GlyphCount);
			QuadInfo.Model = FMatrix::Scale(FVector3(1.f, Rect.Width, Rect.Height)) * FMatrix::Translation(GlyphCenter) * PivotMatrix;
			QuadInfo.Color = mColor;
			QuadInfo.TextureSRV = mFontAtlasAsset->GetSRV();
			QuadInfo.TextureFormat = mFontAtlasAsset->GetFormat();
			QuadInfo.SubUV = FVector4(UV.X, UV.Y, UV.Width, UV.Height);
			QuadInfo.BlendMode = ERenderBlendMode::Transparent;
			QuadInfo.EnableDepthTest = mEnableDepthTest;
			QuadInfo.EnableDepthWrite = mEnableDepthWrite;

			GlyphCount++;
		});

		mRenderProxy->SetActiveRenderTransparentQuadInfoNum(GlyphCount);
	}

	inline void SetText(const std::wstring& text)
	{
		mText = text;
		mbBoundingBoxDirty = true;
		MarkBoundsDirty();
		MarkRenderDirty();
	}

	inline const std::wstring& GetText() const { return mText; }

	inline void SetFontAtlasAsset(const TSharedPtr<FFontAtlasAsset>& fontAtlasAsset) 
	{ 
		mFontAtlasAsset = fontAtlasAsset; 
		mbBoundingBoxDirty = true;
		MarkBoundsDirty();
		MarkRenderDirty();
	}

	inline void SetColor(const FVector4& color)
	{
		mColor = color;
		MarkRenderDirty();
	}

	inline void SetDepthState(bool enableDepthTest, bool enableDepthWrite)
	{
		mEnableDepthTest = enableDepthTest;
		mEnableDepthWrite = enableDepthWrite;
		MarkRenderDirty();
	}

protected:
	virtual FAABB GetBoundingBox() override
	{
		if (mbBoundingBoxDirty)
		{
			mbBoundingBoxDirty = false;

			if (!mFontAtlasAsset)
			{
				mBoundingBox = FAABB();
				return mBoundingBox;
			}

			const TSharedPtr<FFontAtlas>& FontAtlas = mFontAtlasAsset->GetFontAtlas();
			if (!FontAtlas)
			{
				mBoundingBox = FAABB();
				return mBoundingBox;
			}

			FTextBuilder TextBuilder(FontAtlas, WorldUnitPerPixel);

			float TotalWidth = 0.0f;
			float TotalHeight = 0.0f;
			TextBuilder.CalculateSize(mText, TotalWidth, TotalHeight);

			mBoundingBox.Min = FVector(-0.5f, -TotalWidth * 0.5f, -TotalHeight * 0.5f);
			mBoundingBox.Max = FVector(0.5f, TotalWidth * 0.5f, TotalHeight * 0.5f);
			mBoundingBox = mBoundingBox.ToWorld(GetWorldMatrix());
		}

		return mBoundingBox;
	}

	virtual const TArray<FVertex>& GetMeshVertices() const override
	{
		static TArray<FVertex> Vertices = {
			FVertex{ FVector(0.f, -0.5f, 0.5f), FVector(0.f, 0.f, 1.f), mColor, FVector2(0.f, 0.f) },
			FVertex{ FVector(0.f, 0.5f, 0.5f), FVector(0.f, 0.f, 1.f), mColor, FVector2(1.f, 0.f) },
			FVertex{ FVector(0.f, 0.5f, -0.5f), FVector(0.f, 0.f, 1.f), mColor, FVector2(1.f, 1.f) },
			FVertex{ FVector(0.f, -0.5f, -0.5f), FVector(0.f, 0.f, 1.f), mColor, FVector2(0.f, 1.f) },
		};

		return Vertices;
	}

	virtual const TArray<uint32>& GetMeshIndices() const override
	{
		static TArray<uint32> Indices = {
			0, 1, 2,
			0, 2, 3
		};

		return Indices;
	}

	virtual void OnTransformChanged() override
	{
		mbBoundingBoxDirty = true;
		Super::OnTransformChanged();
	}

private:
	std::wstring mText;
	TSharedPtr<FFontAtlasAsset> mFontAtlasAsset;
	FVector4 mColor = FVector4(1, 1, 1, 1);
	bool mEnableDepthTest = true;
	bool mEnableDepthWrite = true;

	bool mbBoundingBoxDirty = true;
	FAABB mBoundingBox;
};

