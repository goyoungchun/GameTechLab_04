#include "EnginePCH.h"
#include "ScriptSystem.h"

FScriptSystem::FScriptSystem()
{
	Init();
}

sol::state& FScriptSystem::GetLua()
{
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
		sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
		sol::meta_function::subtraction, [](const FVector& a, const FVector& b) { return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z); },
		sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; },
		sol::meta_function::to_string, [](const FVector& v) {return std::format("(X : {:.2f}, Y : {:.2f}, Z : {:.2f})", v.X, v.Y, v.Z);}
	);

	Lua->new_usertype<AActor>("Actor",
		sol::no_constructor,
		"UUID", sol::readonly_property(&AActor::GetUUID),
		"Location", sol::property(&AActor::GetActorLocation, &AActor::SetActorLocation),
		"PrintLocation", &AActor::PrintLocation,
		sol::meta_function::to_string, [](const AActor& a) {return std::format("(UUID : {}, Name : {})", a.GetUUID(), a.GetName());}
	);
}

void FScriptSystem::Init()
{
	Lua = MakeUnique<sol::state>();
	OpenLibraries();
	RegisterTypes();
	OverridePrint();
}

void FScriptSystem::Shutdown()
{
	Lua->collect_garbage();
	Lua.reset();
}
