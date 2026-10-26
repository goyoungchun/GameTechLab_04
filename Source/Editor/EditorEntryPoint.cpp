#include "EnginePCH.h"

#include "Launch/EntryPoint.h"

#include "Editor/HitoriEd/EditorEngine.h"
#include "Programs/Benchmark/BenchmarkApp.h"
#include "Programs/ObjViewer/ObjViewerApp.h"
#include "Engine/GameEngine.h"

#include <filesystem>

namespace {
// exe를 Build/Bin 아래에서 바로 실행해도 Assets·Resources·Scenes 상대 경로를 찾도록
// 그 폴더들이 있는 상위 폴더를 작업 디렉터리로 맞춘다.
void SetWorkingDirectoryToProjectRoot() {
  namespace fs = std::filesystem;
  std::error_code ErrorCode;
  if (fs::exists("Assets", ErrorCode) && fs::exists("Resources", ErrorCode))
    return;

  wchar_t ExePath[MAX_PATH]{};
  if (!GetModuleFileNameW(nullptr, ExePath, MAX_PATH))
    return;

  for (fs::path Dir = fs::path(ExePath).parent_path(); !Dir.empty(); Dir = Dir.parent_path()) {
    if (fs::exists(Dir / "Assets", ErrorCode) && fs::exists(Dir / "Resources", ErrorCode)) {
      fs::current_path(Dir, ErrorCode);
      return;
    }
    if (Dir == Dir.root_path())
      return;
  }
}
} // namespace

UClass *GetEngineClass() {
#ifdef OBJ_VIEWER
  return UObjViewerEngine::StaticClass();
#elif GAME_BUILD
  SetWorkingDirectoryToProjectRoot();
  return UGameEngine::StaticClass();
#elif BENCHMARK
  // return UBenchmarkEngine::StaticClass();
#else
  return UEditorEngine::StaticClass();
#endif
}
