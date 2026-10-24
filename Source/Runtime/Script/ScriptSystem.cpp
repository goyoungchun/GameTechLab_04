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
}

void FScriptSystem::RegisterTypes()
{
	Lua->new_usertype<FVector>("Vector",
		sol::call_constructor, sol::constructors<FVector(), FVector(float, float, float)>(),
		"x", &FVector::X,
		"y", &FVector::Y,
		"z", &FVector::Z,
		sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z); },
		sol::meta_function::subtraction, [](const FVector& a, const FVector& b) { return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z); },
		sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; }
	);

	Lua->new_usertype<AActor>("Actor",
		sol::no_constructor,
		"UUID", sol::readonly_property(&AActor::GetUUID),
		"Location", sol::property(&AActor::GetActorLocation, &AActor::SetActorLocation),
		"PrintLocation", &AActor::PrintLocation
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
