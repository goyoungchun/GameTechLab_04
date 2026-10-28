#include "EnginePCH.h"
#include "Editor/HitoriEd/EditorFileUtils.h"

#include "Core/EngineLog.h"
#include "Serialization/JsonArchive.h"
#include "Sequence/SequenceSerializer.h"

#include <shobjidl.h>

FString FEditorFileUtils::CurrentScenePath = "";

class FScopedComInitialization
{
public:
	FScopedComInitialization()
		: Result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))
	{}

	~FScopedComInitialization()
	{
		if (SUCCEEDED(Result))
			CoUninitialize();
	}

	bool IsInitialized() const { return SUCCEEDED(Result); }

private:
	HRESULT Result;
};

bool FEditorFileUtils::NewScene(UWorld* World)
{
	if (!World)
		return false;

	World->ClearWorld();

	// 라인 트래커도 삭제
	World->GetPathTracker().ClearPath();

	CurrentScenePath.clear();

	return true;
}

bool FEditorFileUtils::SaveScene(UWorld* World)
{
	if (!World)
		return false;

	if (CurrentScenePath.empty())
	{
		return SaveSceneAs(World);
	}

	if (!FJsonArchive::SaveWorld(World, CurrentScenePath))
	{
		HTR_LOG(Error, "Failed to save scene: {}", CurrentScenePath);

		MessageBoxW(
			GetActiveWindow(),
			L"씬을 저장하지 못했습니다.\n경로와 쓰기 권한을 확인하세요.",
			L"저장 실패",
			MB_OK | MB_ICONERROR
		);
		return false;
	}
	HTR_LOG(Info, "Save Scene : {}", CurrentScenePath);

	return true;
}

bool FEditorFileUtils::SaveSceneAs(UWorld* World)
{
	if (!World)
		return false;

	FString FilePath = OpenSaveSceneDialog();

	if (FilePath.empty())
		return false;

	FilePath = std::filesystem::absolute(FilePath).lexically_normal().string();

	if (!FJsonArchive::SaveWorld(World, FilePath))
	{
		HTR_LOG(Error, "Failed to save scene: {}", FilePath);

		MessageBoxW(
			GetActiveWindow(),
			L"씬을 저장하지 못했습니다.\n경로와 쓰기 권한을 확인하세요.",
			L"저장 실패",
			MB_OK | MB_ICONERROR
		);

		return false;
	}

	CurrentScenePath = FilePath;

	HTR_LOG(Info, "Save Scene As : {}", CurrentScenePath);

	return true;
}

FString FEditorFileUtils::GetScenePath(UWorld* World)
{
	if (CurrentScenePath.empty())
	{
		SaveSceneAs(World);
	}
	return CurrentScenePath;
}

bool FEditorFileUtils::LoadScene(UWorld* World)
{
	if (!World)
		return false;

	FString FilePath = OpenLoadSceneDialog();

	// 파일 선택 취소
	if (FilePath.empty())
		return false;

	FilePath = std::filesystem::absolute(FilePath).lexically_normal().string();

	if (!FJsonArchive::LoadWorld(World, FilePath))
		return false;

	// 이제 이 파일이 현재 Scene
	CurrentScenePath = FilePath;

	HTR_LOG(Info, "Load Scene : {}", CurrentScenePath);

	return true;
}

struct FFileDialogType
{
	const wchar_t* FilterName;
	const wchar_t* FilterPattern;
	const wchar_t* DefaultExtension;
	std::filesystem::path Folder;
};

bool SetSceneDialogFolder(IFileDialog* Dialog, const std::filesystem::path& RelativeFolder)
{
	std::error_code Error;
	const std::filesystem::path WorkingDirectory = std::filesystem::current_path(Error);
	if (Error)
		return false;

	std::filesystem::path InitialDirectory = (WorkingDirectory / RelativeFolder).make_preferred();
	if (!std::filesystem::is_directory(InitialDirectory, Error))
		InitialDirectory = WorkingDirectory;

	ComPtr<IShellItem> Folder;
	if (FAILED(SHCreateItemFromParsingName(
		InitialDirectory.c_str(), nullptr, IID_PPV_ARGS(Folder.GetAddressOf()))))
	{
		return false;
	}

	// 최근 사용 경로보다 현재 작업 디렉터리 기준의 폴더를 우선한다.
	return SUCCEEDED(Dialog->SetFolder(Folder.Get()));
}

FString GetSceneDialogResult(IFileDialog* Dialog)
{
	ComPtr<IShellItem> SelectedFile;
	if (FAILED(Dialog->GetResult(SelectedFile.GetAddressOf())))
		return FString();

	PWSTR FilePath = nullptr;
	if (FAILED(SelectedFile->GetDisplayName(SIGDN_FILESYSPATH, &FilePath)))
		return FString();

	const std::filesystem::path SelectedPath(FilePath);
	CoTaskMemFree(FilePath);

	return SelectedPath.string();
}

FString ShowSceneDialog(bool bSaveDialog, const FFileDialogType& Type)
{
	FScopedComInitialization ComInitialization;
	if (!ComInitialization.IsInitialized())
	{
		HTR_LOG(Error, "Failed to initialize COM for scene dialog.");
		return FString();
	}

	ComPtr<IFileDialog> Dialog;
	HRESULT Result;
	if (bSaveDialog)
	{
		ComPtr<IFileSaveDialog> SaveDialog;
		Result = CoCreateInstance(
			CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(SaveDialog.GetAddressOf()));
		Dialog = SaveDialog;
	}
	else
	{
		ComPtr<IFileOpenDialog> OpenDialog;
		Result = CoCreateInstance(
			CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(OpenDialog.GetAddressOf()));
		Dialog = OpenDialog;
	}

	if (FAILED(Result))
	{
		HTR_LOG(Error, "Failed to create scene dialog: {}", Result);
		return FString();
	}

	FILEOPENDIALOGOPTIONS Options = {};
	if (FAILED(Dialog->GetOptions(&Options)))
		return FString();

	Options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
	Options |= bSaveDialog ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST;

	const COMDLG_FILTERSPEC FileTypes[] =
	{
		{ Type.FilterName, Type.FilterPattern },
		{ L"All Files (*.*)", L"*.*" }
	};

	if (FAILED(Dialog->SetOptions(Options)) ||
		FAILED(Dialog->SetFileTypes(bSaveDialog ? 1 : 2, FileTypes)) ||
		FAILED(Dialog->SetFileTypeIndex(1)) ||
		FAILED(Dialog->SetDefaultExtension(Type.DefaultExtension)) ||
		!SetSceneDialogFolder(Dialog.Get(), Type.Folder))
	{
		HTR_LOG(Error, "Failed to configure scene dialog.");
		return FString();
	}

	Result = Dialog->Show(GetActiveWindow());
	if (Result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
		return FString();

	if (FAILED(Result))
	{
		HTR_LOG(Error, "Failed to show scene dialog: {}", Result);
		return FString();
	}

	return GetSceneDialogResult(Dialog.Get());
}

FString FEditorFileUtils::OpenSaveSceneDialog()
{
	return ShowSceneDialog(true, FFileDialogType{ L"Scene Files (*.scene)", L"*.scene", L"scene", L"Scenes" });
}

FString FEditorFileUtils::OpenLoadSceneDialog()
{
	return ShowSceneDialog(false, FFileDialogType{ L"Scene Files (*.scene)", L"*.scene", L"scene", L"Scenes" });
}

FString ShowSequenceDialog(bool bSaveDialog)
{
	std::error_code Error;
	std::filesystem::create_directories(FSequenceSerializer::SequenceDirectory, Error);
	return ShowSceneDialog(bSaveDialog, FFileDialogType{ L"Sequence Files (*.sequence)", L"*.sequence", L"sequence", FSequenceSerializer::SequenceDirectory });
}

FString FEditorFileUtils::OpenSaveSequenceDialog()
{
	return ShowSequenceDialog(true);
}

FString FEditorFileUtils::OpenLoadSequenceDialog()
{
	return ShowSequenceDialog(false);
}
