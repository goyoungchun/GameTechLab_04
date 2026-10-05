#include "FEngine.h"

#include "FAssetManager.h"
#include "FFontManager.h"
#include "FileManager.h"
#include "GraphicsManager.h"
#include "NativeFileDialog.h"
#include "ObjectFactory.h"
#include "World.h"
#include "FInstrumentor.h"
#include <format>
#include <stdexcept>

FEngine* GEngine = nullptr;

void FWorldContext::SetCurrentWorld(UWorld* InWorld)
{
	if (InWorld->GetWorldType() != mWorldType)
	{
		throw std::runtime_error(std::format("World type mismatch: expected {}, got {}", static_cast<int>(mWorldType), static_cast<int>(InWorld->GetWorldType())));
		return;
	}

	if (mWorld == InWorld)
	{
		return;
	}

	if (mWorld != nullptr)
	{
		FObjectFactory::DestroyObject(mWorld);
		mWorld = nullptr;
	}

	mWorld = InWorld;
}

void FEngine::Initialize(HINSTANCE hInstance, WNDPROC WndProc)
{
	PROFILE_BEGIN_SESSION("Engine", "engine-loop-profile.json");
	PROFILE_FUNCTION();

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Initialize window infos
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
	RegisterClassW(&wndclass);

	mHWnd = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 1024,
		nullptr, nullptr, hInstance, nullptr
	);

	// 창을 화면 크기에 맞게 최대화하여 표시
	ShowWindow(mHWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(mHWnd);

	// 최대화된 후의 실제 클라이언트 크기를 구해 콘솔에 전달
	RECT clientRect;
	GetClientRect(mHWnd, &clientRect);
	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;		// Generic Desktop
	rid.usUsage = 0x02;			// Mouse
	rid.dwFlags = 0;		// 포커스 있을 때만 수신
	rid.hwndTarget = mHWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	mGraphicsManager = new FGraphicsManager(mHWnd);
	FNativeFileDialog::Initialize(mHWnd);

	const FVector4 NearTint(1.0f, 0.65f, 0.15f, 0.85f); // 주황 = 가까운 쪽
	const FVector4 FarTint(0.25f, 0.55f, 1.0f, 0.85f); // 파랑 = 먼 쪽

	mFileManager = new FFileManager();
	mFontManager = new FFontManager();
	mAssetManager = new FAssetManager();
}

void FEngine::Cleanup()
{
	PROFILE_SCOPE("FEngineLoop::End");
	delete mFileManager;
	delete mAssetManager;
	delete mFontManager;
	delete mGraphicsManager;
}

FWorldContext& FEngine::CreateNewWorldContext(EWorldType WorldType)
{
	FWorldContext& NewContext = mWorldContexts.Emplace();
	NewContext.mWorldType = WorldType;

	OnCreateWorldContext(NewContext);

	return NewContext;
}

void FEngine::DestroyWorldContext(FWorldContext* Context)
{
	DestroyWorldContext(Context->mWorld);
}

void FEngine::DestroyWorldContext(UWorld* InWorld)
{
	for (int32 i = 0; i < mWorldContexts.Num(); ++i)
	{
		if (mWorldContexts[i].mWorld == InWorld)
		{
			OnDestroyWorldContext(mWorldContexts[i]);
			FObjectFactory::DestroyObject(InWorld);
			mWorldContexts.RemoveAtSwap(i);
			return;
		}
	}

	throw std::runtime_error("World context not found for the given world.");
}

