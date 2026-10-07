#include "FPropertyWindow.h"
#include "Vector.h"
#include "ImGui/imgui.h"
#include "Actor.h"
#include "Transform.h"
#include "UTextComponent.h"
#include "UAtlasAnimationComponent.h"
#include "FLogManager.h"
#include "UStaticMeshComponent.h"
#include "FAssetManager.h"
#include "AssetDragDrop.h"
#include "FEditorUIManager.h"
#include "FEditorEngine.h"
#include "Assets.h"
#include "SceneComponent.h"
#include "ActorComponent.h"

void FPropertyWindow::Render(const FGuiReference& GuiReference)
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Jungle Property Window", nullptr, flags);

	UActorComponent* TargetComponent = GuiReference.SceneManager->GetSelectedComponent();

	if (TargetComponent)
	{
		mAssetManager = GuiReference.AssetManager;
		mSelectedComponent = TargetComponent;
		mSelectedActor = mSelectedComponent->GetOwner();

		const TSet<UActorComponent*>& Components = mSelectedActor->GetComponents();
		
		char ActorNameBuffer[256];
		FString ActorName = mSelectedActor->GetName().ToString();
		std::strcpy(ActorNameBuffer, ActorName.c_str());
		if (ImGui::InputText("Actor Name", ActorNameBuffer, sizeof(ActorNameBuffer)))
		{
			mSelectedActor->Rename(FName(ActorNameBuffer));
		}

		ImGui::SameLine();
		
		if (ImGui::Button("+ Add"))
		{
			ImGui::OpenPopup("Add Component");
		}

		bool bDestroyed = false;
		if (mSelectedComponent && mSelectedComponent != mSelectedActor->GetRootComponent())
		{
			ImGui::SameLine();

			if (ImGui::Button("- Remove"))
			{
				bDestroyed = true;
			}
		}

		RenderAddComponentPopup();
		
		if (ImGui::TreeNode(std::format("{} (Instance)", ActorName).c_str()))
		{
			USceneComponent* RootComponent = mSelectedActor->GetRootComponent();
			if (RootComponent)
			{
				RenderSceneComponentHierarchy(RootComponent);
			}

			for (UActorComponent* Component : Components)
			{
				if (Component->IsA<USceneComponent>() || Component->IsEditorOnly())
				{
					continue;
				}

				bool bSelected = mSelectedComponent == Component;

				ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf;
				NodeFlags |= bSelected ? ImGuiTreeNodeFlags_Selected : 0;

				bool Open = ImGui::TreeNodeEx(Component->GetName().ToString().c_str(), NodeFlags);

				if (ImGui::IsItemClicked())
				{
					mSelectedComponent = Component;
				}

				if (Open)
				{
					ImGui::TreePop();
				}
			}

			ImGui::TreePop();
		}

		if (mSelectedComponent)
		{
			ImGui::Separator();

			char ComponentNameBuffer[256];
			FString ComponentName = mSelectedComponent->GetName().ToString();
			std::strcpy(ComponentNameBuffer, ComponentName.c_str());
			if (ImGui::InputText("Component Name", ComponentNameBuffer, sizeof(ComponentNameBuffer)))
			{
				mSelectedComponent->Rename(FName(ComponentNameBuffer));
			}

			if (mSelectedComponent->IsA<USceneComponent>())
			{
				ImGui::SeparatorText(mSelectedComponent->GetClass()->Name.c_str());

				RenderTransformProperties(mSelectedComponent->Cast<USceneComponent>());
			}

			ImGui::SeparatorText(mSelectedComponent->GetClass()->Name.c_str());

			if (mSelectedComponent->IsA<UText3DComponent>())
			{
				RenderText3DComponent(mSelectedComponent->Cast<UText3DComponent>());
			}
			else if (mSelectedComponent->IsA<USpotLightComponent>())
			{
				RenderSpotLightComponent(mSelectedComponent->Cast<USpotLightComponent>());
			}
			else if (mSelectedComponent->IsA<UAtlasAnimationComponent>())
			{
				RenderAtlasAnimationComponent(mSelectedComponent->Cast<UAtlasAnimationComponent>());
			}
			else if (mSelectedComponent->IsA<UStaticMeshComponent>())
			{
				RenderStaticMeshComponent(mSelectedComponent->Cast<UStaticMeshComponent>());
			}
			else if (mSelectedComponent->IsA<UHeightFogComponent>())
			{
				RenderHeightFogComponent(mSelectedComponent->Cast<UHeightFogComponent>());
			}
			else if (mSelectedComponent->IsA<UPointLightComponent>())
			{
				RenderPointLightComponent(mSelectedComponent->Cast<UPointLightComponent>());
			}
			else if (mSelectedComponent->IsA<UTextRenderComponent>())
			{
				RenderTextRenderComponent(mSelectedComponent->Cast<UTextRenderComponent>());
			}
			else if (mSelectedComponent->IsA<UProjectileMovementComponent>())
			{
				RenderProjectileMovementComponent(mSelectedComponent->Cast<UProjectileMovementComponent>());
			}
			else if (mSelectedComponent->IsA<URotationMovementComponent>())
			{
				RenderRotationMovementComponent(mSelectedComponent->Cast<URotationMovementComponent>());
			}
			else if (mSelectedComponent->IsA<UBillboardComponent>())
			{
				RenderBillboardComponent(mSelectedComponent->Cast<UBillboardComponent>());
			}
		}

		if (mSelectedComponent != TargetComponent)
		{
			GuiReference.SceneManager->SetSelectedComponent(mSelectedComponent);
		}

		if (bDestroyed)
		{
			mSelectedActor->RemoveComponent(mSelectedComponent);
			mSelectedComponent->DestroyComponent();
			GuiReference.SceneManager->ResetSelectedComponent();
			GuiReference.SceneManager->SetSelectedComponent(mSelectedActor->GetRootComponent());
		}
	}
	else
	{
		mSelectedActor = nullptr;
		mSelectedComponent = nullptr;
	}

	ImGui::End();
}

void FPropertyWindow::RenderAddComponentPopup()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(240, 0), ImVec2(240, 300)); // 최대 높이 300

	auto MakeUniqueName = [](const FString& BaseName, const TSet<UActorComponent*>& ExistingComponents) -> FString
	{
		int32 Suffix = 1;

		FString UniqueName = BaseName;
		while (true)
		{
			bool bIsUnique = true;
			for (UActorComponent* Component : ExistingComponents)
			{
				if (Component->GetName() == UniqueName)
				{
					bIsUnique = false;
					break;
				}
			}

			if (bIsUnique)
			{
				break;
			}

			UniqueName = std::format("{}_{}", BaseName.ToString(), Suffix++);
		}

		return UniqueName;
	};

	if (ImGui::BeginPopup("Add Component"))
	{
		if (ImGui::MenuItem("StaticMeshComponent"))
		{
			UStaticMeshComponent* NewComponent = mSelectedActor->CreateDefaultSubobject<UStaticMeshComponent>(FName(MakeUniqueName("StaticMeshComponent", mSelectedActor->GetComponents())));

			if (mSelectedComponent && mSelectedComponent->IsA<USceneComponent>())
			{
				NewComponent->SetupAttachment(mSelectedComponent->Cast<USceneComponent>(), false);
			}

			mSelectedActor->AddOwnedComponent(NewComponent);

			ImGui::CloseCurrentPopup();
		}

		if (ImGui::MenuItem("BillboardComponent"))
		{
			UBillboardComponent* NewComponent = mSelectedActor->CreateDefaultSubobject<UBillboardComponent>(FName(MakeUniqueName("BillboardComponent", mSelectedActor->GetComponents())));

			if (mSelectedComponent && mSelectedComponent->IsA<USceneComponent>())
			{
				NewComponent->SetupAttachment(mSelectedComponent->Cast<USceneComponent>(), false);
			}

			mSelectedActor->AddOwnedComponent(NewComponent);

			ImGui::CloseCurrentPopup();
		}

		if (ImGui::MenuItem("ProjectileMovementComponent"))
		{
			UProjectileMovementComponent* NewComponent = mSelectedActor->CreateDefaultSubobject<UProjectileMovementComponent>(FName(MakeUniqueName("ProjectileMovementComponent", mSelectedActor->GetComponents())));

			mSelectedActor->AddOwnedComponent(NewComponent);

			ImGui::CloseCurrentPopup();
		}

		if (ImGui::MenuItem("RotationMovementComponent"))
		{
			URotationMovementComponent* NewComponent = mSelectedActor->CreateDefaultSubobject<URotationMovementComponent>(FName(MakeUniqueName("RotationMovementComponent", mSelectedActor->GetComponents())));

			mSelectedActor->AddOwnedComponent(NewComponent);

			ImGui::CloseCurrentPopup();
		}

		if (ImGui::MenuItem("PointLightComponent"))
		{
			UPointLightComponent* NewComponent = mSelectedActor->CreateDefaultSubobject<UPointLightComponent>(FName(MakeUniqueName("PointLightComponent", mSelectedActor->GetComponents())));

			if (mSelectedComponent && mSelectedComponent->IsA<USceneComponent>())
			{
				NewComponent->SetupAttachment(mSelectedComponent->Cast<USceneComponent>(), false);
			}

			mSelectedActor->AddOwnedComponent(NewComponent);

			ImGui::CloseCurrentPopup();
		}

		if (ImGui::MenuItem("TextRenderComponent"))
		{
			UTextRenderComponent* NewComponent = mSelectedActor->CreateDefaultSubobject<UTextRenderComponent>(FName(MakeUniqueName("TextRenderComponent", mSelectedActor->GetComponents())));

			if (mSelectedComponent && mSelectedComponent->IsA<USceneComponent>())
			{
				NewComponent->SetupAttachment(mSelectedComponent->Cast<USceneComponent>(), false);
			}

			mSelectedActor->AddOwnedComponent(NewComponent);
			NewComponent->SetFontAtlasAsset(FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FName("TestFontAtlas"), true));

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();

	}
}

void FPropertyWindow::RenderSceneComponentHierarchy(USceneComponent* SceneComponent)
{
	if (mSelectedActor != SceneComponent->GetOwner())
	{
		// 서로 다른 owner를 가지거나 EditorOnly인 SceneComponent는 렌더링하지 않습니다.
		return;
	}
	
	const TArray<USceneComponent*> ChildComponents = SceneComponent->GetChildComponents();
	
	TArray<USceneComponent*> ValidChildComponents;
	for (USceneComponent* Component : ChildComponents)
	{
		if (!Component->IsEditorOnly())
		{
			ValidChildComponents.Add(Component);
		}
	}

	bool bIsSelected = mSelectedComponent == SceneComponent;
	bool bHasChildren = !ValidChildComponents.IsEmpty();

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
	NodeFlags |= bIsSelected ? ImGuiTreeNodeFlags_Selected : 0;
	NodeFlags |= bHasChildren ? 0 : ImGuiTreeNodeFlags_Leaf;

	bool Open = ImGui::TreeNodeEx(std::format("{}", SceneComponent->GetName().ToString().c_str()).c_str(), NodeFlags);

	if (ImGui::IsItemClicked())
	{
		mSelectedComponent = SceneComponent;
	}

	if (Open)
	{
		for (USceneComponent* Component : ValidChildComponents)
		{
			RenderSceneComponentHierarchy(Component);
		}

		ImGui::TreePop();
	}
}

void FPropertyWindow::RenderTransformProperties(USceneComponent* SceneComponent)
{
	FVector translationInput = SceneComponent->GetRelativeLocation();
	FRotator rotationInput = ToEulerAngles(SceneComponent->GetRelativeRotation());
	FVector scaleInput = SceneComponent->GetRelativeScale3D();

	if (ImGui::DragFloat3("Translation", &translationInput.x, 0.1f))
	{
		SceneComponent->SetRelativeLocation(translationInput);
	}

	float SwizzleRotationInput[3] = { rotationInput.Roll, rotationInput.Pitch, rotationInput.Yaw };
	if (ImGui::DragFloat3("Rotation", SwizzleRotationInput, 0.1f))
	{
		rotationInput.Pitch = SwizzleRotationInput[1];
		rotationInput.Yaw = SwizzleRotationInput[2];
		rotationInput.Roll = SwizzleRotationInput[0];

		SceneComponent->SetRelativeRotation(ToQuaternion(rotationInput));
	}

	if (ImGui::DragFloat3("Scale", &scaleInput.x, 0.1f, MIN_SCALE, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
	{
		SceneComponent->SetRelativeScale3D(scaleInput);
	}
}

void FPropertyWindow::RenderText3DComponent(UText3DComponent* text3DComponent)
{
	char textBuffer[256] = {};
	const FString currentText = Wide2Utf(text3DComponent->GetText());
	strncpy_s(textBuffer, currentText.CStr(), sizeof(textBuffer) - 1);

	if (ImGui::InputText("Display Text", textBuffer, sizeof(textBuffer)))
	{
		text3DComponent->SetText(Utf2Wide(FString(textBuffer)));
	}
}

void FPropertyWindow::RenderSpotLightComponent(USpotLightComponent* spotLightComponent)
{
	FVector4 colorInput = spotLightComponent->GetColor();
	if (ImGui::ColorPicker3("Color", &colorInput.x,
		ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex))
	{
		spotLightComponent->SetColor(colorInput);
	}

	float innerAngleInput = spotLightComponent->GetInnerConeAngle();
	if (ImGui::DragFloat("InnerAngle", &innerAngleInput, 0.1f, 0.f, spotLightComponent->GetOuterConeAngle(), "%.3f", ImGuiSliderFlags_AlwaysClamp))
	{
		spotLightComponent->SetInnerConeAngle(innerAngleInput);
	}

	float outerAngleInput = spotLightComponent->GetOuterConeAngle();
	if (ImGui::DragFloat("OuterAngle", &outerAngleInput, 0.1f, 0.f, 89.f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
	{
		spotLightComponent->SetOuterConeAngle(outerAngleInput);
	}
}

void FPropertyWindow::RenderAtlasAnimationComponent(UAtlasAnimationComponent* atlasAnimationComponent)
{
	TArray<FString> spriteAtlasAssetNames;
	mAssetManager->ForEachMetaInfo([&spriteAtlasAssetNames](const FAssetMetaInfo& metaInfo) {
		if (metaInfo.AssetType != EAssetType::SpriteAtlas)
		{
			return;
		}
		spriteAtlasAssetNames.Add(metaInfo.AssetName.ToString());
		});

	const TSharedPtr<FSpriteAtlasAsset>& currentAtlas = atlasAnimationComponent->GetAtlas();
	FString currentAtlasName = currentAtlas ? currentAtlas->GetAssetName().ToString() : "None";
	if (ImGui::BeginCombo("Sprite Atlas", currentAtlasName.CStr()))
	{
		for (const FString& assetName : spriteAtlasAssetNames)
		{
			bool isSelected = (currentAtlasName == assetName);
			if (ImGui::Selectable(assetName.CStr(), isSelected))
			{
				atlasAnimationComponent->SetAtlas(mAssetManager->GetAssetAs<FSpriteAtlasAsset>(FName(assetName), true));
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Play"))
	{
		atlasAnimationComponent->Play(0, atlasAnimationComponent->IsLooping(), atlasAnimationComponent->IsBackward());
	}
	ImGui::SameLine();
	if (ImGui::Button("Pause"))
	{
		atlasAnimationComponent->Pause();
	}
	ImGui::SameLine();
	if (ImGui::Button("Resume"))
	{
		atlasAnimationComponent->Resume();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		atlasAnimationComponent->Reset();
	}

	ImGui::Text(atlasAnimationComponent->IsPlaying() ? "State: Playing" : "State: Stopped");

	bool loopInput = atlasAnimationComponent->IsLooping();
	if (ImGui::Checkbox("bLoop", &loopInput))
	{
		atlasAnimationComponent->SetLooping(loopInput);
	}

	int32 frameRateInput = atlasAnimationComponent->GetFrameRate();
	if (ImGui::DragInt("FrameRate", &frameRateInput, 1.f, 1, 240, "%d", ImGuiSliderFlags_AlwaysClamp))
	{
		atlasAnimationComponent->SetFrameRate(frameRateInput);
	}
}

void FPropertyWindow::RenderStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent)
{
	TSharedPtr<FStaticMeshAsset> CurrentStaticMesh = StaticMeshComponent->GetMesh();

	TArray<FString> StaticMeshAssetNames;
	TArray<FAssetMetaInfo> materialMetaInfos;
	mAssetManager->ForEachMetaInfo([&StaticMeshAssetNames, &materialMetaInfos](const FAssetMetaInfo& metaInfo) {
		if (metaInfo.AssetType == EAssetType::StaticMesh)
		{
			StaticMeshAssetNames.Add(metaInfo.AssetName.ToString());
		}
		else if (metaInfo.AssetType == EAssetType::Material)
		{
			materialMetaInfos.Add(metaInfo);
		}
		});

	const FString CurrentMeshPath = CurrentStaticMesh ? CurrentStaticMesh->GetAssetName().ToString() : "None";

	// Path에서 확장자 빼고 파일명만 parsing 하여 보여주기
	std::filesystem::path meshPath = std::filesystem::path(static_cast<std::string>(CurrentMeshPath)).stem();
	FString simpleMeshName = meshPath.stem().string();

	if (ImGui::BeginCombo("Static Mesh", simpleMeshName.CStr()))
	{
		for (const FString& assetName : StaticMeshAssetNames)
		{
			bool isSelected = (CurrentMeshPath == assetName);

			// Path에서 확장자 빼고 파일명만 parsing 하여 보여주기
			std::filesystem::path assetPath = std::filesystem::path(static_cast<std::string>(assetName)).stem();
			FString simpleAssetName = assetPath.stem().string();

			if (ImGui::Selectable((simpleAssetName.ToString() + "##" + assetName.ToString()).c_str(), isSelected))
			{
				StaticMeshComponent->SetMesh(mAssetManager->GetAssetAs<FStaticMeshAsset>(FName(assetName), true));
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", assetName.CStr());

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ASSET_GUID"))
		{
			const FGuid& AssetGuid = *static_cast<const FGuid*>(Payload->Data);
			const FAssetMetaInfo& AssetMetaInfo = mAssetManager->GetMetaInfo(AssetGuid);

			if (AssetMetaInfo.AssetType == EAssetType::StaticMesh)
			{
				TSharedPtr<FStaticMeshAsset> MatchedMeshAsset = mAssetManager->GetAssetAs<FStaticMeshAsset>(AssetGuid, true);
				StaticMeshComponent->SetMesh(MatchedMeshAsset);
				UE_LOG("Success: StaticMesh applied: %s", AssetGuid.ToString().c_str());
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 현재 가진 Material이 있으면 그것을, 없으면 None을 콤보박스 이름으로
	const auto& Materials = StaticMeshComponent->GetMaterials();
	for (int32 i = 0; i < Materials.Num(); i++)
	{
		ImGui::PushID(i);

		TSharedPtr<FMaterialAsset> currentMaterial = Materials[i];
		FString currentMaterialName = currentMaterial ? currentMaterial->GetAssetName().ToString() : "None";

		// Path에서 확장자 빼고 파일명만 parsing 하여 보여주기
		std::filesystem::path materialPath = std::filesystem::path(static_cast<std::string>(currentMaterialName)).stem();
		FString simpleMaterialName = materialPath.stem().string();

		if (ImGui::BeginCombo("Material", simpleMaterialName.CStr()))
		{
			for (const FAssetMetaInfo& metaInfo : materialMetaInfos)
			{
				// Path에서 확장자 빼고 파일명만 parsing 하여 보여주기
				std::filesystem::path metaPath = std::filesystem::path(metaInfo.AssetName.ToString().ToString()).stem();
				FString simpleMetaPath = metaPath.stem().string();

				bool isSelected = (currentMaterialName == metaInfo.AssetName.ToString());
				if (ImGui::Selectable((simpleMetaPath.ToString() + "##" + metaInfo.AssetID.ToString().ToString()).c_str(), isSelected))
				{
					TSharedPtr<FMaterialAsset> materialAsset = mAssetManager->GetAssetAs<FMaterialAsset>(metaInfo.AssetID, true);
					StaticMeshComponent->SetMaterial(i, materialAsset);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", metaInfo.AssetName.ToString().CStr()); if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		FVector2 UVOffset = StaticMeshComponent->GetUVOffset(i);
		if (ImGui::DragFloat2("UV Offset", &UVOffset.X, 0.01f))
		{
			StaticMeshComponent->SetUVOffset(i, UVOffset);
		}
		ImGui::PopID();
	}
}

void FPropertyWindow::RenderHeightFogComponent(UHeightFogComponent* HeightFogComponent)
{
	float FogDensity = HeightFogComponent->GetFogDensity();
	if (ImGui::DragFloat("Fog Density", &FogDensity, 0.01f, 0.0f, 1.0f))
	{
		HeightFogComponent->SetFogDensity(FogDensity);
	}

	float FogHeightFalloff = HeightFogComponent->GetFogHeightFalloff();
	if (ImGui::DragFloat("Fog Height Falloff", &FogHeightFalloff, 0.01f, 0.0f, 1.0f))
	{
		HeightFogComponent->SetFogHeightFalloff(FogHeightFalloff);
	}

	float FogStartDistance = HeightFogComponent->GetFogStartDistance();
	if (ImGui::DragFloat("Fog Start Distance", &FogStartDistance, 1.0f, 0.0f, 10000.0f))
	{
		HeightFogComponent->SetFogStartDistance(FogStartDistance);
	}

	float FogCutoffDistance = HeightFogComponent->GetFogCutoffDistance();
	if (ImGui::DragFloat("Fog Cutoff Distance", &FogCutoffDistance, 1.0f, 0.0f, 10000.0f))
	{
		HeightFogComponent->SetFogCutoffDistance(FogCutoffDistance);
	}

	float FogMaxOpacity = HeightFogComponent->GetFogMaxOpacity();
	if (ImGui::DragFloat("Fog Max Opacity", &FogMaxOpacity, 0.01f, 0.0f, 1.0f))
	{
		HeightFogComponent->SetFogMaxOpacity(FogMaxOpacity);
	}

	float FogHeight = HeightFogComponent->GetFogHeight();
	if (ImGui::DragFloat("Fog Height", &FogHeight, 1.0f, -10000.0f, 10000.0f))
	{
		HeightFogComponent->SetFogHeight(FogHeight);
	}

	FLinearColor FogInscatteringColor = HeightFogComponent->GetFogInscatteringColor();
	if (ImGui::ColorPicker3("Fog Inscattering Color", &FogInscatteringColor.R, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex))
	{
		HeightFogComponent->SetFogInscatteringColor(FogInscatteringColor);
	}
}

void FPropertyWindow::RenderPointLightComponent(UPointLightComponent* PointLightComponent)
{
	FLinearColor ColorInput = PointLightComponent->GetColor();
	if (ImGui::ColorPicker3("Color", ColorInput.V, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex))
	{
		PointLightComponent->SetColor(ColorInput);
	}

	float IntensityInput = PointLightComponent->GetIntensity();
	if (ImGui::DragFloat("Intensity", &IntensityInput, 0.1f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
	{
		PointLightComponent->SetIntensity(IntensityInput);
	}

	float RadiusInput = PointLightComponent->GetRadius();
	if (ImGui::DragFloat("Radius", &RadiusInput, 0.1f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
	{
		PointLightComponent->SetRadius(RadiusInput);
	}

	float RadiusFallOffInput = PointLightComponent->GetRadiusFallOff();
	if (ImGui::DragFloat("Radius Falloff", &RadiusFallOffInput, 0.1f, 0.001f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp))
	{
		PointLightComponent->SetRadiusFallOff(RadiusFallOffInput);
	}
}

void FPropertyWindow::RenderTextRenderComponent(UTextRenderComponent* TextRenderComponent)
{
	char textBuffer[1024 * 16] = {};
	const FString currentText = Wide2Utf(TextRenderComponent->GetText());
	strncpy_s(textBuffer, currentText.CStr(), sizeof(textBuffer) - 1);

	if (ImGui::InputTextMultiline("Display Text", textBuffer, sizeof(textBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16)))
	{
		TextRenderComponent->SetText(Utf2Wide(FString(textBuffer)));
	}
}

void FPropertyWindow::RenderProjectileMovementComponent(UProjectileMovementComponent* ProjectileMovementComponent)
{
	FVector VelocityInput = ProjectileMovementComponent->GetVelocity();
	if (ImGui::DragFloat3("Velocity", &VelocityInput.x, 0.1f))
	{
		ProjectileMovementComponent->SetVelocity(VelocityInput);
	}
}

void FPropertyWindow::RenderRotationMovementComponent(URotationMovementComponent* RotationMovementComponent)
{
	FVector RotationAxisInput = RotationMovementComponent->GetRotationAxis();
	if (ImGui::DragFloat3("Rotation Axis", &RotationAxisInput.x, 0.1f))
	{
		RotationMovementComponent->SetRotationAxis(RotationAxisInput);
	}

	float RotationSpeedInput = RotationMovementComponent->GetRotationSpeed();
	if (ImGui::DragFloat("Rotation Speed", &RotationSpeedInput, 0.1f))
	{
		RotationMovementComponent->SetRotationSpeed(RotationSpeedInput);
	}
}

void FPropertyWindow::RenderBillboardComponent(UBillboardComponent* BillboardComponent)
{
	TSharedPtr<FTexture2DAsset> CurrentTexture = BillboardComponent->GetTexture();

	FString CurrentTextureName = CurrentTexture ? CurrentTexture->GetAssetName().ToString() : "NONE";
	if (ImGui::BeginCombo("TextureAsset", CurrentTextureName.CStr()))
	{
		TArray<FString> TextureNames;
		mAssetManager->ForEachMetaInfo([&TextureNames](const FAssetMetaInfo& metaInfo)
			{
				if (metaInfo.AssetType == EAssetType::Texture2D)
				{
					TextureNames.Add(metaInfo.AssetName.ToString());
				}
			});
		for (const FString& TextureName : TextureNames)
		{
			bool isSelected = (CurrentTextureName == TextureName);

			if (ImGui::Selectable(TextureName.CStr(), isSelected))
			{
				TSharedPtr<FTexture2DAsset> newTexture = mAssetManager->GetAssetAs<FTexture2DAsset>(FName(TextureName), true);
				BillboardComponent->SetTexture(newTexture);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TextureName.CStr());

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	//ERenderBlendMode CurrentBlendMode = BillboardComponent->GetRenderBlendMode();
	//if (ImGui::BeginCombo("BlendMode", BlendModeNames[static_cast<int>(CurrentBlendMode)].CStr()))
	//{
	//	for (int i = 0; i < BlendModeNames.Num() ; i++)
	//	for (const FString& BlendMode : BlendModeNames)
	//	{
	//		bool isSelected = (CurrentBlendMode == ERenderBlendMode);

	//		if (ImGui::Selectable(TextureName.CStr(), isSelected))
	//		{
	//			TSharedPtr<FTexture2DAsset> newTexture = mAssetManager->GetAssetAs<FTexture2DAsset>(FName(TextureName), true);
	//			BillboardComponent->SetTexture(newTexture);
	//		}
	//		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TextureName.CStr());

	//		if (isSelected)
	//		{
	//			ImGui::SetItemDefaultFocus();
	//		}
	//	}
	//	ImGui::EndCombo();
	//}

	FVector4 SubUV = BillboardComponent->GetSubUV();
	if (ImGui::DragFloat4("SubUV", &SubUV.x, 0.05f))
	{
		BillboardComponent->SetSubUV(SubUV);
	}

	FVector2 SubUVOffset = BillboardComponent->GetSubUVOffset();
	if (ImGui::DragFloat2("SubUVOffset", &SubUVOffset.X, 0.05f))
	{
		BillboardComponent->SetSubUVOffset(SubUVOffset);
	}

	bool EnableDepthTest = BillboardComponent->GetEnbaleDepthTest();
	if (ImGui::Checkbox("EnableDepthTest", &EnableDepthTest))
	{
		BillboardComponent->SetEnbaleDepthTest(EnableDepthTest);
	}

	bool EnableDepthWrite = BillboardComponent->GetEnbaleDepthWrite();
	if (ImGui::Checkbox("EnableDepthWrite", &EnableDepthWrite))
	{
		BillboardComponent->SetEnbaleDepthWrite(EnableDepthWrite);
	}
}
