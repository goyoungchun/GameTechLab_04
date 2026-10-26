#include "EnginePCH.h"
#include "Input/InputSystem.h"
#include "ScriptSystem.h"
#include "Component/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "Asset/AssetManager.h"
#include "Component/ScriptComponent.h"
#include "Core/EngineLog.h"

FScriptSystem::FScriptSystem()
{
}

sol::state& FScriptSystem::GetLua()
{
	assert(bIsInitialized && "FScriptSystem::Init() not called");
	return *Lua;
}

void FScriptSystem::OpenLibraries()
{
	Lua->open_libraries(sol::lib::base);
	Lua->open_libraries(sol::lib::math);
	Lua->open_libraries(sol::lib::string);
	Lua->open_libraries(sol::lib::table);
	Lua->open_libraries(sol::lib::coroutine);
	Lua->open_libraries(sol::lib::jit);
}

void FScriptSystem::OverridePrint()
{
	Lua->set_function("print", [this](sol::variadic_args Args)
		{
			sol::protected_function ToString = (*Lua)["tostring"];

			FString Line;
			for (auto Arg : Args)
			{
				if (!Line.empty())
					Line += "\t";

				auto Result = ToString(Arg.get<sol::object>());
				Line += Result.valid() ? Result.get<FString>() : "<?>";
			}
			HTR_LOG(Info, "Lua : {}", Line);
		});

	Lua->set_function("LogError", [this](sol::variadic_args Args)
		{
			sol::protected_function ToString = (*Lua)["tostring"];

			FString Line;
			for (auto Arg : Args)
			{
				if (!Line.empty())
					Line += "\t";

				auto Result = ToString(Arg.get<sol::object>());
				Line += Result.valid() ? Result.get<FString>() : "<?>";
			}
			HTR_LOG(Error, "Lua : {}", Line);
		});

	Lua->set_function("LogWarning", [this](sol::variadic_args Args)
		{
			sol::protected_function ToString = (*Lua)["tostring"];

			FString Line;
			for (auto Arg : Args)
			{
				if (!Line.empty())
					Line += "\t";

				auto Result = ToString(Arg.get<sol::object>());
				Line += Result.valid() ? Result.get<FString>() : "<?>";
			}
			HTR_LOG(Warning, "Lua : {}", Line);
		});
}


// Lua 스크립트에서 C++ 타입을 쓸 수 있도록 등록한다.
//   new_usertype<T>("이름", ...)  : Lua에서 "이름"으로 T를 쓸 수 있게 한다. 뒤에는 "Lua에서 부를 이름", "연결할 C++ 멤버"를 짝지어 나열한다
//   sol::call_constructor + constructors : Lua에서 Vector(1, 2, 3)처럼 바로 만들 수 있게 한다
//   sol::no_constructor           : Lua에서는 만들 수 없게 막는다 (엔진이 넘겨준 것만 쓰게)
//   &T::변수                       : Lua에서 v.x 처럼 읽고 쓸 수 있다
//   &T::함수                       : Lua에서 obj:함수() 처럼 부를 수 있다
//   sol::property(Get, Set)        : Lua에서는 변수처럼 보이지만, 읽을 땐 Get, 값을 넣을 땐 Set이 불린다
//   sol::readonly_property(Get)    : 읽기만 되는 변수. 값을 넣으려 하면 Lua 에러
//   sol::meta_function::addition 등 : Lua의 + - * 연산을 했을 때 실행할 함수
void FScriptSystem::RegisterTypes()
{
	Lua->new_usertype<FVector>("Vector",
		sol::call_constructor, sol::constructors<FVector(), FVector(float, float, float)>(),
		"x", &FVector::X,
		"y", &FVector::Y,
		"z", &FVector::Z,
		"Length", &FVector::Length,
		"Normalized", &FVector::Normalized,
		"Dot", [](const FVector& a, const FVector& b) { return a.Dot(b); },
		"Cross", [](const FVector& a, const FVector& b) { return a.Cross(b); },
		"Distance", [](const FVector& a, const FVector& b) { return FVector::Distance(a, b); },
		sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
		sol::meta_function::subtraction, [](const FVector& a, const FVector& b) { return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z); },
		sol::meta_function::multiplication, sol::overload(                          // v * 2, 2 * v 둘 다
			[](const FVector& v, float f) { return v * f; },
			[](float f, const FVector& v) { return v * f; }),
		sol::meta_function::division, [](const FVector& v, float f) { return v / f; },
		sol::meta_function::unary_minus, [](const FVector& v) { return FVector(-v.X, -v.Y, -v.Z); },
		sol::meta_function::equal_to, [](const FVector& a, const FVector& b) { return a == b; },
		sol::meta_function::to_string, [](const FVector& v) {return std::format("(X : {:.2f}, Y : {:.2f}, Z : {:.2f})", v.X, v.Y, v.Z);}
	);

	Lua->new_usertype<FRotator>("Rotator",
		sol::call_constructor, sol::constructors<FRotator(), FRotator(float, float, float)>(),
		"Pitch", &FRotator::Pitch, "Yaw", &FRotator::Yaw, "Roll", &FRotator::Roll,
		sol::meta_function::to_string, [](const FRotator& r) { return std::format("(P : {:.1f}, Y : {:.1f}, R : {:.1f})", r.Pitch, r.Yaw, r.Roll); }
	);

	// 화면 UI 색. Color(r, g, b) 또는 Color(r, g, b, a), 값은 0~1.
	Lua->new_usertype<FVector4>("Color",
		sol::call_constructor, sol::factories(
			[](float R, float G, float B) { return FVector4(R, G, B, 1.0f); },
			[](float R, float G, float B, float A) { return FVector4(R, G, B, A); }
		),
		"r", &FVector4::X, "g", &FVector4::Y, "b", &FVector4::Z, "a", &FVector4::W,
		sol::meta_function::to_string, [](const FVector4& Color) 
		{ return std::format("(R : {:.2f}, G : {:.2f}, B : {:.2f}, A : {:.2f})", Color.X, Color.Y, Color.Z, Color.W); }
	);

	sol::table ColorTable = (*Lua)["Color"];
	ColorTable["White"] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorTable["Black"] = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	ColorTable["Red"] = FVector4(1.0f, 0.2f, 0.2f, 1.0f);
	ColorTable["Green"] = FVector4(0.2f, 1.0f, 0.2f, 1.0f);
	ColorTable["Blue"] = FVector4(0.2f, 0.6f, 1.0f, 1.0f);
	ColorTable["Yellow"] = FVector4(1.0f, 0.9f, 0.1f, 1.0f);
	ColorTable["Orange"] = FVector4(1.0f, 0.6f, 0.0f, 1.0f);
	ColorTable["Transparent"] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	Lua->new_usertype<AActor>("Actor",
		sol::no_constructor,
		"UUID", sol::readonly_property(&AActor::GetUUID),
		"Location", sol::property(&AActor::GetActorLocation, &AActor::SetActorLocation),
		"PrintLocation", &AActor::PrintLocation,
		"Name", sol::readonly_property(&AActor::GetName),
		"Rotation", sol::property(&AActor::GetActorRotation, &AActor::SetActorRotation),
		"Scale", sol::property(&AActor::GetActorScale3D, &AActor::SetActorScale3D),
		"IsActorBeingDestroyed", &AActor::IsActorBeingDestroyed,
		"FindActor", [](AActor& Self, const FString& Name) -> AActor*
		{
			UWorld* World = Self.GetWorld();
			if (!World || !World->GetPersistentLevel()) return nullptr;

			const FName Target(Name);
			for (AActor* Actor : World->GetPersistentLevel()->GetActors())
			{
				if (Actor && !Actor->IsActorBeingDestroyed() && Actor->GetFName() == Target)
					return Actor;
			}
			return nullptr;
		},
		"SpawnActor", [](AActor& Self, const FString& ScriptPath, const FVector& Location,
			sol::optional<FString> MeshName) -> AActor*
		{
			UWorld* World = Self.GetWorld();
			if (!World)
			{
				return nullptr;
			}

			FTransform Transform;
			Transform.Location = Location;
			AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(NAME_None, &Transform);
			if (!NewActor)
			{
				return nullptr;
			}

			// 이름을 인자로 넣으면 해당 메시 아니면 호출한 액터의 메시로 초기화한다
			UStaticMesh* Mesh = nullptr;
			if (MeshName)
			{
				Mesh = UAssetManager::GetMesh(*MeshName);
			}
			else if (AStaticMeshActor* SelfMeshActor = Cast<AStaticMeshActor>(&Self))
			{
				Mesh = SelfMeshActor->GetStaticMeshComponent()->GetStaticMesh();
			}
			if (Mesh)
			{
				NewActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			}

			// 스크립트경로를 인자로 넣으면 스크립트컴포넌트를 붙인다. 플레이 중 스폰이라 다음 Tick에 BeginPlay가 호출된다
			if (!ScriptPath.empty())
			{
				UScriptComponent* Script = NewActor->CreateDefaultSubobject<UScriptComponent>("LuaScript");
				Script->SetScriptPath(ScriptPath);
				Script->RegisterComponent();
			}
			return NewActor;
		},
		// 이미 있는 StaticMeshActor의 메시를 이름으로 바꾼다
		"SetMesh", [](AActor& Self, const FString& MeshName) -> bool
		{
			AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(&Self);
			UStaticMesh* Mesh = UAssetManager::GetMesh(MeshName);
			if (!MeshActor || !Mesh)
			{
				return false;
			}
			MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			return true;
		},
		"SetViewCamera", [](AActor& Self, int32 PlayerIndex, const FVector& Location, const FRotator& Rotation)
		{
			if (UWorld* World = Self.GetWorld())
			{
				World->SetViewCameraOverride(PlayerIndex - 1, Location, Rotation);
			}
		},
		"ClearViewCamera", [](AActor& Self, int32 PlayerIndex)
		{
			if (UWorld* World = Self.GetWorld())
			{
				World->ClearViewCameraOverride(PlayerIndex - 1);
			}
		},
		"SetText", [](AActor& Self, const FString& InText)
		{
			for (UActorComponent* Component : Self.GetComponents())
			{
				if (UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(Component))
				{
					TextComponent->SetText(InText);
					return true;
				}
			}
			return false;
		},
		"GetText", [](AActor& Self) -> FString
		{
			for (UActorComponent* Component : Self.GetComponents())
			{
				if (UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(Component))
				{
					return TextComponent->GetText();
				}
			}
			return FString();
		},
		sol::meta_function::to_string, [](const AActor& a) {return std::format("(UUID : {}, Name : {})", a.GetUUID(), a.GetName());}
	);

	// UI.Text의 가로 정렬. 기준점(x)이 글자의 왼쪽 끝·가운데·오른쪽 끝에 온다.
	Lua->new_enum("Align", "Left", ETextAlign::Left, "Center", ETextAlign::Center, "Right", ETextAlign::Right);
}

void FScriptSystem::RegisterInput()
{
	Lua->new_enum("Key",
		"W", EKeyCode::W, "A", EKeyCode::A, "S", EKeyCode::S, "D", EKeyCode::D,
		"Q", EKeyCode::Q, "E", EKeyCode::E, "R", EKeyCode::R, "F", EKeyCode::F,
		"Up", EKeyCode::Up, "Down", EKeyCode::Down, "Left", EKeyCode::Left, "Right", EKeyCode::Right,
		"Space", EKeyCode::Space, "Enter", EKeyCode::Enter, "Escape", EKeyCode::Escape,
		"Shift", EKeyCode::Shift, "Control", EKeyCode::Control, "Tab", EKeyCode::Tab,
		"LShift", EKeyCode::LShift, "RShift", EKeyCode::RShift, "LControl", EKeyCode::LControl, "RControl", EKeyCode::RControl,
		"Num0", EKeyCode::Num0, "Num1", EKeyCode::Num1, "Num2", EKeyCode::Num2, "Num3", EKeyCode::Num3,
		"Num4", EKeyCode::Num4, "Num5", EKeyCode::Num5, "Num6", EKeyCode::Num6, "Num7", EKeyCode::Num7,
		"Num8", EKeyCode::Num8, "Num9", EKeyCode::Num9, "Backspace", EKeyCode::Backspace
	);

	Lua->set_function("IsKeyDown", [](EKeyCode Key) { return FInputSystem::IsKeyDown(Key); });
	Lua->set_function("IsKeyPressed", [](EKeyCode Key) { return FInputSystem::IsKeyPressed(Key); });
	Lua->set_function("IsKeyReleased", [](EKeyCode Key) { return FInputSystem::IsKeyReleased(Key); });

	Lua->new_enum("Mouse",
		"Left", EMouseButton::Left, "Right", EMouseButton::Right, "Middle", EMouseButton::Middle
	);
	Lua->set_function("IsMouseDown", [](EMouseButton Button) { return FInputSystem::IsMouseDown(Button); });
	Lua->set_function("IsMousePressed", [](EMouseButton Button) { return FInputSystem::IsMousePressed(Button); });
	Lua->set_function("IsMouseReleased", [](EMouseButton Button) { return FInputSystem::IsMouseReleased(Button); });
}

void FScriptSystem::RegisterFunction()
{
	Lua->set_function("RestartLevel", []() {
		if (GEngine)
		{
			GEngine->RequestRestartLevel();
		}
	});

	// 에디터에서는 PIE만 끝나고, 게임 실행 파일에서는 프로그램이 종료된다.
	Lua->set_function("QuitGame", []() {
		if (GEngine)
		{
			GEngine->RequestQuitGame();
		}
	});
}

void FScriptSystem::LoadCoroutineWait()
{
	const char* ScriptPath = "Assets/Scripts/coroutine_waits.lua";
	
	try
	{
		Lua->script_file(ScriptPath);
	}
	catch (const sol::error& Err)
	{
		HTR_LOG(Error, "[FScriptSystem] LoadCoroutineWait failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void FScriptSystem::Init()
{
	if (bIsInitialized)
	{
		return;
	}
	Lua = MakeUnique<sol::state>();
	OpenLibraries();
	RegisterTypes();
	RegisterInput();
	RegisterFunction();
	OverridePrint();

	Lua->create_named_table("Game",
		"Score", 0,
		"State", "Ready"
	);
	bIsInitialized = true;
	LoadCoroutineWait();
}

void FScriptSystem::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}
	Lua->collect_garbage();
	Lua.reset();

	bIsInitialized = false;
}
