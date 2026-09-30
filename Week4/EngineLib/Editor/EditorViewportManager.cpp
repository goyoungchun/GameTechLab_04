#include "EditorViewportManager.h"
#include "FEditorViewportClient.h"
#include "../Core/Math/MathUtility.h"

//Todo : 종료하기전 패널 모드를 유지해서 불러와야함
bool FEditorViewportManager::Initialize(FAssetManager& assetManager)
{
	mAssetManager = &assetManager;

	if (!Viewports.IsEmpty())
	{
		for (const std::unique_ptr<FViewport>& viewport : Viewports)
		{
			viewport->getClient().Initialize(assetManager);
		}
		return true;
	}
	return addViewport() != invalidViewportId;
}
void FEditorViewportManager::applyLayoutSetting(const FEditorSetting& setting)
{
	mEditorSetting.viewportCount = FMath::Clamp<uint8>(setting.viewportCount,1,maxViewportCount);
	mEditorSetting.twoPaneVertical = FMath::Clamp(setting.twoPaneVertical, 0.0f, 1.0f);
	mEditorSetting.threePaneVertical = FMath::Clamp(setting.threePaneVertical, 0.0f, 1.0f);
	mEditorSetting.threePaneRightHorizontal = FMath::Clamp(setting.threePaneRightHorizontal,0.0f,1.0f);
	mEditorSetting.fourPaneHorizontal = FMath::Clamp(setting.fourPaneHorizontal, 0.0f, 1.0f);
	mEditorSetting.fourPaneVertical = FMath::Clamp(setting.fourPaneVertical, 0.0f, 1.0f);

	const uint8 targetCount = mEditorSetting.viewportCount;

	while (getViewportCount() < targetCount)
	{
		if (addViewport() == invalidViewportId)
		{
			break;
		}
	}

	while (getViewportCount() > targetCount)
	{
		if (!removeViewport(getViewportCount()))
		{
			break;
		}
	}

	for (uint8 index = 0;index < getViewportCount();++index)
	{
		FViewport* viewport = getViewportAt(index);

		if (viewport == nullptr)
		{
			continue;
		}

		const FViewportSetting& saved = setting.Viewports[index];
		viewport->setType(saved.Type);

		FViewportRenderSettings& renderSettings = viewport->getRenderSettings();

		renderSettings.ViewMode = saved.ViewMode;
		renderSettings.ShowFlags = saved.ShowFlags;
	}

	rebuildLayout();
}

void FEditorViewportManager::captureLayoutSetting(FEditorSetting& outSetting) const
{
	outSetting.viewportCount = getViewportCount();

	outSetting.twoPaneVertical = mEditorSetting.twoPaneVertical;
	outSetting.threePaneVertical = mEditorSetting.threePaneVertical;
	outSetting.threePaneRightHorizontal = mEditorSetting.threePaneRightHorizontal;
	outSetting.fourPaneHorizontal = mEditorSetting.fourPaneHorizontal;
	outSetting.fourPaneVertical = mEditorSetting.fourPaneVertical;

	if (outSetting.Viewports.Num() < maxViewportCount)
	{
		outSetting.Viewports.SetNum(maxViewportCount);
	}

	for (uint8 index = 0; index < getViewportCount(); ++index)
	{
		const FViewport* viewport = getViewportAt(index);

		if (viewport == nullptr)
		{
			continue;
		}

		FViewportSetting& saved = outSetting.Viewports[index];

		saved.Type = viewport->getType();
		saved.ViewMode = viewport->getRenderSettings().ViewMode;
		saved.ShowFlags = viewport->getRenderSettings().ShowFlags;
	}
}

FViewport* FEditorViewportManager::getActiveViewport()
{
	return findViewport(activeViewportId);
}
const FViewport* FEditorViewportManager::getActiveViewport() const
{
	return findViewport(activeViewportId);
}
FViewport* FEditorViewportManager::getViewportAt(uint8 index)
{
	if (index >= static_cast<uint8>(Viewports.Num()))
	{
		return nullptr;
	}
	return Viewports[index].get();
}
const FViewport* FEditorViewportManager::getViewportAt(uint8 index) const
{
	if (index >= static_cast<uint8>(Viewports.Num()))
	{
		return nullptr;
	}
	return Viewports[index].get();
}
bool FEditorViewportManager::setActiveViewport(uint8 viewportId)
{
	if (findViewport(viewportId) == nullptr)
	{
		return false;
	}
	activeViewportId = viewportId;
	return true;
}
EViewportLayoutMode FEditorViewportManager::getLayoutMode() const
{
	return layoutMode;
}
uint8 FEditorViewportManager::getViewportCount() const
{
	return Viewports.Num();
}
std::unique_ptr<SWindow> FEditorViewportManager::makeViewportPanel(uint8 index)
{
	if (index >= static_cast<uint8>(Viewports.Num()))
	{
		return nullptr;
	}
	return std::make_unique<SViewportPanel>(*Viewports[index]);
}
std::unique_ptr<SWindow> FEditorViewportManager::makeSingleLayout()
{
	return makeViewportPanel(0);
}
std::unique_ptr<SWindow> FEditorViewportManager::makeTwoPaneLayout()
{
	auto root = std::make_unique<SSplitterV>(makeViewportPanel(0), makeViewportPanel(1), mEditorSetting.twoPaneVertical);
	mPrimarySplitter = root.get();
	return root;
}
std::unique_ptr<SWindow> FEditorViewportManager::makeThreePaneLayout()
{
	auto rightSide = std::make_unique<SSplitterH>(makeViewportPanel(1), makeViewportPanel(2), mEditorSetting.threePaneRightHorizontal);
	mSecondarySplitter = rightSide.get();
	auto root = std::make_unique<SSplitterV>(makeViewportPanel(0), std::move(rightSide), mEditorSetting.threePaneVertical);
	mPrimarySplitter = root.get();
	return root;
}
std::unique_ptr<SWindow> FEditorViewportManager::makeFourPaneLayout()
{
	auto top = std::make_unique<SSplitterV>(makeViewportPanel(0), makeViewportPanel(1), mEditorSetting.fourPaneVertical);
	auto bottom = std::make_unique<SSplitterV>(makeViewportPanel(2), makeViewportPanel(3), mEditorSetting.fourPaneVertical);

	top->linkSplitRatio(*bottom);

	mSecondarySplitter = top.get();

	auto root = std::make_unique<SSplitterH>(std::move(top), std::move(bottom), mEditorSetting.fourPaneHorizontal);
	mPrimarySplitter = root.get();

	return root;
	/*
	auto bottom1 = std::make_unique<SSplitterH>(makeViewportPanel(2), makeViewportPanel(3), 0.5f);
	auto bottom = std::make_unique<SSplitterH>(makeViewportPanel(1), std::move(bottom1), 0.33f);
	return std::make_unique<SSplitterV>(makeViewportPanel(0), std::move(bottom), 0.5f);
	*/
}
uint8 FEditorViewportManager::addViewport()
{
	if (mAssetManager == nullptr)
	{
		return invalidViewportId;
	}

	const uint8 viewportId = allocateViewportId();
	if (viewportId <= invalidViewportId)
	{
		return invalidViewportId;
	}
	// 뷰포트 추가시 시점 선정 순서 선언
	static constexpr EViewportType defaultTypes[] =
	{
		EViewportType::Perspective,
		EViewportType::Top,
		EViewportType::Front,
		EViewportType::Right
	};
	
	auto viewport = std::make_unique<FViewport>(
		viewportId, defaultTypes[viewportId - 1], mSharedSettings);
	viewport->getClient().Initialize(*mAssetManager);
	Viewports.Add(std::move(viewport));
	if (activeViewportId == invalidViewportId)
	{
		activeViewportId = viewportId;
	}
	rebuildLayout();
	return viewportId;
}
uint8 FEditorViewportManager::allocateViewportId() const
{
	if (Viewports.Num() >= maxViewportCount)
	{
		return invalidViewportId;
	}
	return static_cast<uint8>(Viewports.Num() + 1);
}
bool FEditorViewportManager::removeViewport(uint8_t viewportId)
{
	if (Viewports.Num() <= 1)
	{
		return false;
	}
	const int8_t index = findViewportIndex(viewportId);
	if (index < 0)
	{
		return false;
	}
	FViewport* activeViewport = getActiveViewport();
	const bool removedActive = activeViewport == Viewports[index].get();

	draggingSplitter = nullptr;
	layoutRoot.reset();
	Viewports.RemoveAt(static_cast<uint8>(index), 1);
	reindexViewports();

	if (removedActive)
	{
		activeViewportId = 1;
	}
	else if (activeViewport != nullptr)
	{
		activeViewportId = activeViewport->getId();
	}

	rebuildLayout();

	return true;
}
void FEditorViewportManager::reindexViewports()
{
	for (uint32 index = 0; index < Viewports.Num(); index++)
	{
		// 뷰포트 id는 1,2,3,4 이므로 index에 1을 더함
		Viewports[index]->setId(static_cast<uint8>(index + 1)); 
	}
}
FViewport* FEditorViewportManager::findViewport(uint8 viewportId)
{
	const int8 index = findViewportIndex(viewportId);
	if (index < 0)
	{
		return nullptr;
	}
	return Viewports[index].get();
}
const FViewport* FEditorViewportManager::findViewport(uint8 viewportId) const
{
	const int8 index = findViewportIndex(viewportId);
	if (index < 0)
	{
		return nullptr;
	}
	return Viewports[index].get();
}
int8 FEditorViewportManager::findViewportIndex(uint8 viewportId) const
{
	if (viewportId == invalidViewportId)
	{
		return -1;
	}
	for (int8 index = 0; index < Viewports.Num(); index++)
	{
		const std::unique_ptr<FViewport>& viewport = Viewports[index];
		if (viewport && viewport->getId() == viewportId)
		{
			return index;
		}
	}
	return -1;
}
void FEditorViewportManager::rebuildLayout()
{
	draggingSplitter = nullptr;
	layoutRoot.reset();

	const uint8 viewportCount = Viewports.Num();

	switch (viewportCount)
	{
	case 1 :
		layoutMode = EViewportLayoutMode::SinglePane;
		layoutRoot = makeSingleLayout();
		break;
	case 2 :
		layoutMode = EViewportLayoutMode::TwoPane;
		layoutRoot = makeTwoPaneLayout();
		break;
	case 3 :
		layoutMode = EViewportLayoutMode::ThreePane;
		layoutRoot = makeThreePaneLayout();
		break;
	case 4 :
		layoutMode = EViewportLayoutMode::FourPane;
		layoutRoot = makeFourPaneLayout();
		break;
	default:
		layoutRoot.reset();
		break;
	}
}
void FEditorViewportManager::arrangeLayout(const FRect& hostRect)
{
	if (layoutRoot == nullptr)
	{
		return;
	}
	if (hostRect.getWidth() <= 0.0f || hostRect.getHeight() <= 0.0f)
	{
		return;
	}

	layoutRoot->Arrange(hostRect);
}
void FEditorViewportManager::setCameraSpeed(float speed)
{
	mSharedSettings.cameraSpeed = speed;
}
void FEditorViewportManager::setSnapPreset(uint8_t presetIndex)
{
	if (presetIndex >= FViewportSharedSettings::snapPresets.size()) { return; }
	mSharedSettings.snapPresetIndex = presetIndex;
}
bool FEditorViewportManager::beginSplitterDrag(const FPoint& point)
{
	if (layoutRoot == nullptr)
	{
		draggingSplitter = nullptr;
		return false;
	}

	// 반드시 arrangeLayout()이 한 번 호출된 뒤 사용해야 한다.
	draggingSplitter = layoutRoot->findSplitter(point);

	return draggingSplitter != nullptr;
}
void FEditorViewportManager::updateSplitterDrag(const FPoint& point)
{
	if (draggingSplitter == nullptr)
	{
		return;
	}
	draggingSplitter->updateSplitFromPoint(point);
}
bool FEditorViewportManager::endSplitterDrag()
{
	if (draggingSplitter == nullptr)
	{
		return false;
	}

	switch (layoutMode)
	{
	case EViewportLayoutMode::TwoPane:
		mEditorSetting.twoPaneVertical = mPrimarySplitter->getSplitRatio();
		break;
	case EViewportLayoutMode::ThreePane:
		mEditorSetting.threePaneRightHorizontal = mSecondarySplitter->getSplitRatio();
		mEditorSetting.threePaneVertical = mPrimarySplitter->getSplitRatio();
		break;
	case EViewportLayoutMode::FourPane:
		mEditorSetting.fourPaneVertical = mSecondarySplitter->getSplitRatio();
		mEditorSetting.fourPaneHorizontal = mPrimarySplitter->getSplitRatio();
		break;
	default:
		break;
	}
	mEditorSetting.viewportCount = getViewportCount();
	draggingSplitter = nullptr;
	return true;
}

FCamera* FEditorViewportManager::getPerspectiveCamera()
{
	for (uint8 index = 0;index < getViewportCount();++index)
	{
		FViewport* viewport = getViewportAt(index);

		if (viewport != nullptr &&viewport->getType() == EViewportType::Perspective)
		{
			return &viewport->getClient().GetCamera();
		}
	}
	return nullptr;
}
const FCamera* FEditorViewportManager::getPerspectiveCamera() const
{
	for (uint8 index = 0;index < getViewportCount();++index)
	{
		const FViewport* viewport = getViewportAt(index);

		if (viewport != nullptr && viewport->getType() == EViewportType::Perspective)
		{
			return &viewport->getClient().GetCamera();
		}
	}
	return nullptr;
}

bool FEditorViewportManager::applyPerspectiveCamera(const FCamera& camera)
{
	bool applied = false;
	for (uint8 index = 0; index < getViewportCount(); index++)
	{
		FViewport* viewport = getViewportAt(index);
		if (viewport == nullptr || viewport->getType() != EViewportType::Perspective)
		{
			continue;
		}
		FEditorViewportClient& client = viewport->getClient();
		client.setViewportSettings(EViewportType::Perspective);
		FCamera& Camera = client.GetCamera();

		Camera.Location = camera.Location;
		Camera.Rotation = camera.Rotation;
		Camera.mFovDegree = camera.mFovDegree;
		Camera.Velocity = camera.Velocity;
		Camera.mFarZ = camera.mFarZ;
		Camera.mNearZ = camera.mNearZ;
		applied = true;
	}
	return applied;
}

void FEditorViewportManager::resetPerspectiveCamera()
{
	const FCamera defaultCamera;
	applyPerspectiveCamera(defaultCamera);
}


/*
void FEditorViewportManager::updateViewports(float deltaTime, FSceneManager& sceneManager)
{

}
void FEditorViewportManager::renderViewports(FGraphicsManager& graphicsManager, FSceneManager& sceneManager)
{

}
*/
