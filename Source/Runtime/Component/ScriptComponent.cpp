#include "EnginePCH.h"
#include "ScriptComponent.h"
#include "GameFramework/Actor.h"
#include "Core/EngineLog.h"
#include "Engine/World.h"
#include "UI/GameUIBuffer.h"
#include "Asset/AssetManager.h"
#include "Render/Texture/Texture2D.h"
#include "Container/Set.h"
#include "Component/PrimitiveComponent.h"
#include "Collision/HitResult.h"

namespace
{
	UTexture2D* FindUITexture(const FString& Path)
	{
		static TSet<FString> FailedPaths;
		if (FailedPaths.Contains(Path))
			return nullptr;

		UTexture2D* Texture = UAssetManager::Get().LoadTexture(Path, false);
		if (!Texture || !Texture->GetResource())
		{
			FailedPaths.Add(Path);
			HTR_LOG(Warning, "[UI] Image not found: {}", Path);
			return nullptr;
		}
		return Texture;
	}
}

UScriptComponent::UScriptComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UScriptComponent::~UScriptComponent()
{

}

bool UScriptComponent::LoadScriptFile(sol::environment InEnvironment)
{
	if (ScriptPath.empty())
	{
		return false;
	}
	sol::state& Lua = FScriptSystem::Get().GetLua();
	InEnvironment = sol::environment(Lua, sol::create, Lua.globals());
	BindScriptAPI(InEnvironment);

	if (!ResolveScriptPath())
	{
		UnbindScriptAPI(InEnvironment);
		return false;
	}

	auto Result = Lua.safe_script_file(ScriptPath, InEnvironment, sol::script_pass_on_error);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] Load failed: {} ({})", ScriptPath, Err.what());
		ClearCoroutines();
		UnbindScriptAPI(InEnvironment);
		return false;
	}

	CacheScriptCallbacks(InEnvironment);

	std::error_code ErrorCode;
	LastFileWriteTime = fs::last_write_time(ScriptPath, ErrorCode);
	if (ErrorCode)
	{
		HTR_LOG(Error, "[Script] Last Write Time Check failed: {} ({})", ScriptPath, ErrorCode.message());
	}

	Environment = InEnvironment;
	Lua.collect_garbage();
	return true;
}

void UScriptComponent::CacheScriptCallbacks(sol::environment& InEnvironment)
{
	LuaBeginPlay = InEnvironment["BeginPlay"];
	LuaTick = InEnvironment["Tick"];
	LuaEndPlay = InEnvironment["EndPlay"];
	LuaOnOverlap = InEnvironment["OnOverlap"];
	LuaOnHit = InEnvironment["OnHit"];
}

void UScriptComponent::ClearScriptCallbacks()
{
	LuaBeginPlay	= sol::nil;
	LuaTick			= sol::nil;
	LuaOnOverlap	= sol::nil;
	LuaOnHit		= sol::nil;
	LuaEndPlay		= sol::nil;
}

void UScriptComponent::BindScriptAPI(sol::environment& InEnvironment)
{
	InEnvironment["obj"] = GetOwner();
	BindCoroutineAPI(InEnvironment);
	BindUIAPI(InEnvironment);
}

void UScriptComponent::BindCoroutineAPI(sol::environment& InEnvironment)
{
	InEnvironment.set_function("StartCoroutine", [this](sol::protected_function Fn, sol::variadic_args Args)
		{
			return StartCoroutine(Fn, Args);
		}
	);
	InEnvironment.set_function("StopCoroutine", [this](sol::optional<uint32> Id)
		{
			if (!Id)
				return;
			if (FCoroutineRecord* Record = FindRecord(*Id))
				StopRecord(*Record);
		}
	);
	InEnvironment.set_function("StopAllCoroutines", [this]() { StopAllRecords(); });
}

FGameUIBuffer* UScriptComponent::GetGameUIBuffer() const
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	return World ? &World->GetGameUIBuffer() : nullptr;
}

void UScriptComponent::BindUIAPI(sol::environment& InEnvironment)
{
	sol::state& Lua = FScriptSystem::Get().GetLua();
	sol::table UITable = Lua.create_table();

	// UI.Text(x, y, text, [size], [color], [align], [font]) — y는 글자 기준선
	UITable.set_function("Text", [this](float X, float Y, const FString& Text,
		sol::optional<float> Size, sol::optional<FVector4> Color, sol::optional<ETextAlign> Align, sol::optional<FString> Font)
		{
			if (FGameUIBuffer* Buffer = GetGameUIBuffer())
			{
				Buffer->AddText({ X, Y }, Text, Size.value_or(32.0f), Color.value_or(FVector4(1, 1, 1, 1)),
					Align.value_or(ETextAlign::Left), Font.value_or(FString()));
			}
		});

	// UI.Rect(x, y, w, h, [color]) — (x, y)는 왼쪽 위
	UITable.set_function("Rect", [this](float X, float Y, float SizeX, float SizeY, sol::optional<FVector4> Color)
		{
			if (FGameUIBuffer* Buffer = GetGameUIBuffer())
			{
				Buffer->AddRect({ X, Y }, { SizeX, SizeY }, Color.value_or(FVector4(1, 1, 1, 1)));
			}
		});

	// UI.Bar(x, y, w, h, ratio, [color], [bgColor]) — ratio는 0~1
	UITable.set_function("Bar", [this](float X, float Y, float SizeX, float SizeY, float Ratio,
		sol::optional<FVector4> Color, sol::optional<FVector4> BackgroundColor)
		{
			if (FGameUIBuffer* Buffer = GetGameUIBuffer())
			{
				Buffer->AddBar({ X, Y }, { SizeX, SizeY }, Ratio, Color.value_or(FVector4(1, 1, 1, 1)),
					BackgroundColor.value_or(FVector4(0, 0, 0, 0.5f)));
			}
		});

	// UI.Image(x, y, w, h, path, [color]) — 지정한 크기로 늘려 그린다. color는 이미지에 곱해진다.
	UITable.set_function("Image", [this](float X, float Y, float SizeX, float SizeY, const FString& Path,
		sol::optional<FVector4> Color)
		{
			FGameUIBuffer* Buffer = GetGameUIBuffer();
			UTexture2D* Texture = Buffer ? FindUITexture(Path) : nullptr;
			if (Texture)
			{
				Buffer->AddImage({ X, Y }, { SizeX, SizeY }, Texture->GetResource(), Color.value_or(FVector4(1, 1, 1, 1)));
			}
		});

	// UI.ImageFit(x, y, h, path, [color]) — 높이만 정하고 폭은 원본 비율을 따른다.
	UITable.set_function("ImageFit", [this](float X, float Y, float SizeY, const FString& Path,
		sol::optional<FVector4> Color)
		{
			FGameUIBuffer* Buffer = GetGameUIBuffer();
			UTexture2D* Texture = Buffer ? FindUITexture(Path) : nullptr;
			if (Texture)
			{
				Buffer->AddImage({ X, Y }, { 0.0f, SizeY }, Texture->GetResource(), Color.value_or(FVector4(1, 1, 1, 1)),
					FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), true);
			}
		});

	// UI.ImageUV(x, y, w, h, path, u0, v0, u1, v1, [color]) — 이미지의 일부(UV 0~1, 왼쪽 위 기준)만 그린다.
	UITable.set_function("ImageUV", [this](float X, float Y, float SizeX, float SizeY, const FString& Path,
		float U0, float V0, float U1, float V1, sol::optional<FVector4> Color)
		{
			FGameUIBuffer* Buffer = GetGameUIBuffer();
			UTexture2D* Texture = Buffer ? FindUITexture(Path) : nullptr;
			if (Texture)
			{
				Buffer->AddImage({ X, Y }, { SizeX, SizeY }, Texture->GetResource(), Color.value_or(FVector4(1, 1, 1, 1)),
					FVector2(U0, V0), FVector2(U1, V1));
			}
		});

	// local w, h = UI.GetImageSize(path) — 원본 픽셀 크기. 없으면 0, 0
	UITable.set_function("GetImageSize", [](const FString& Path)
		{
			UTexture2D* Texture = FindUITexture(Path);
			return std::make_tuple(Texture ? Texture->GetWidth() : 0u, Texture ? Texture->GetHeight() : 0u);
		});

	// UI.BeginPlayer(index) ~ UI.EndPlayer() 사이의 명령은 해당 플레이어 화면 기준으로 그린다.
	UITable.set_function("BeginPlayer", [this](int32 PlayerIndex)
		{
			if (FGameUIBuffer* Buffer = GetGameUIBuffer())
			{
				Buffer->BeginPlayer(PlayerIndex);
			}
		});

	UITable.set_function("EndPlayer", [this]()
		{
			if (FGameUIBuffer* Buffer = GetGameUIBuffer())
			{
				Buffer->EndPlayer();
			}
		});

	InEnvironment["UI"] = UITable;
}

void UScriptComponent::UnbindUIAPI(sol::environment& InEnvironment)
{
	if (!InEnvironment.valid())
		return;

	InEnvironment["UI"] = sol::lua_nil;
}

void UScriptComponent::UnbindScriptAPI(sol::environment& InEnvironment)
{
	if (!InEnvironment.valid())
		return;

	UnbindCoroutineAPI(InEnvironment);
	UnbindUIAPI(InEnvironment);
	InEnvironment["obj"] = sol::lua_nil;
}

void UScriptComponent::UnbindCoroutineAPI(sol::environment& InEnvironment)
{
	if (!InEnvironment.valid())
		return;
	
	InEnvironment["StartCoroutine"] = sol::lua_nil;
	InEnvironment["StopCoroutine"] = sol::lua_nil;
	InEnvironment["StopAllCoroutines"] = sol::lua_nil;
}

void UScriptComponent::ClearCoroutines()
{
	StopAllRecords();
	CoroutineRecords.Reset();
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();
	bAcceptCoroutines = true;
	sol::environment NewEnvironment;
	LoadScriptFile(NewEnvironment);
	BindCollisionEvents();

	if (!LuaBeginPlay.valid() || GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}
	auto Result = LuaBeginPlay();
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaBeginPlay failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::EndPlay()
{	
	bAcceptCoroutines = false;
	UnbindCollisionEvents();

	if (LuaEndPlay.valid())
	{
		auto Result = LuaEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			HTR_LOG(Error, "[Script] LuaEndPlay failed: {} ({})", ScriptPath, Err.what());
		}
	}

	ClearCoroutines();
	UnbindScriptAPI(Environment);
	Environment = sol::lua_nil;
	ClearScriptCallbacks();
	Super::EndPlay();
}

namespace 
{
	bool EqualsIgnoreCase(const FString& A, const FString& B)
	{
		uint64 Index = 0;
		for (; Index < A.size() && Index < B.size(); ++Index)
		{
			const int Left = std::tolower(static_cast<unsigned char>(A[Index]));
			const int Right = std::tolower(static_cast<unsigned char>(B[Index]));
			if (Left != Right)
				return false;
		}
		return Index == A.size() && Index == B.size();
	}
}


bool UScriptComponent::FindScriptByFileName(fs::path FileName)
{
	std::error_code ErrorCode;
	bool FindScript = false;

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(
		ScriptFolderPath,
		fs::directory_options::skip_permission_denied,
		ErrorCode
	))
	{
		if (Entry.is_regular_file())
		{
			fs::path Path = Entry.path().generic_string();
			if (Path.extension() == ".lua" && EqualsIgnoreCase(Path.filename().generic_string(), FileName.generic_string()))
			{
				if (!FindScript)
				{
					FString FolderPath(ScriptFolderPath);
					SetScriptPath(FolderPath + fs::relative(Path, ScriptFolderPath, ErrorCode).generic_string());
					FindScript = true;
				}
				else
				{
					HTR_LOG(Warning, "[Script] Duplicate script name '{}': using '{}', ignored '{}'", 
						FileName.generic_string(), ScriptPath, Path.generic_string());
					return FindScript;
				}
			}
		}
	}
	return FindScript;
}

bool UScriptComponent::ResolveScriptPath()
{ 
	std::error_code ErrorCode;
	fs::path Path = ScriptPath;

	if(Path.empty())
	{
		return false;
	}


	if (!fs::exists(Path, ErrorCode) || ErrorCode)
	{
		if (bScriptNotFound || !FindScriptByFileName(Path.filename()))
		{
			bScriptNotFound = true;
			return false;
		}
	}
	bScriptNotFound = false;
	return true;
}

bool UScriptComponent::CheckChangeScriptFile(float DeltaTime)
{
	std::error_code ErrorCode;

	HotReloadCheckTimer += DeltaTime;
	if (HotReloadCheckTimer < 0.5f)
	{
		return false;
	}

	HotReloadCheckTimer = 0;

	if (!ResolveScriptPath())
	{
		return false;
	}

	uintmax_t FileSize = fs::file_size(ScriptPath, ErrorCode);
	if (ErrorCode || FileSize == 0)
	{
		return false;
	}

	fs::file_time_type CurrentFileWriteTime = 
		fs::last_write_time(ScriptPath, ErrorCode);

	if (ErrorCode)
	{
		HTR_LOG(Error, "[Script] Last Write Time Check failed: {} ({})", ScriptPath, ErrorCode.message());
		return false;
	}
	
	if (CurrentFileWriteTime != LastFileWriteTime)
	{
		return true;
	}
	return false;
}

bool UScriptComponent::HotReloadFile()
{
	sol::environment OldEnvironment = Environment;
	TArray<TUniquePtr<FCoroutineRecord>> OldRecords(std::move(CoroutineRecords));

	sol::environment NewEnvironment;
	if (LoadScriptFile(NewEnvironment))
	{
		OldRecords.Reset();
		UnbindScriptAPI(OldEnvironment);
		return true;
	}

	CoroutineRecords = std::move(OldRecords);
	return false;
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}

	++FrameCounter;

	if (CheckChangeScriptFile(DeltaTime))
	{
		HotReloadFile();
	}

	if (LuaTick.valid())
	{
		auto Result = LuaTick(DeltaTime);
		if (!Result.valid())
		{
			sol::error Error = Result;
			if (!(FString(Error.what()) == LastTickError))
			{
				HTR_LOG(Error, "[Script] LuaTick failed: {} ({})", ScriptPath, Error.what());
				LastTickError = FString(Error.what());
			}
		}
	}
	
	UpdateCoroutines(DeltaTime);
}

void UScriptComponent::OnHit(AActor *InOtherActor, const FVector& InNormal)
{
	if (!LuaOnHit.valid() || GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}
	auto Result = LuaOnHit(InOtherActor, InNormal);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaOnHit failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::OnOverlap(AActor* InOtherActor)
{
	if (!LuaOnOverlap.valid() || GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}
	auto Result = LuaOnOverlap(InOtherActor);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaOnOverlap failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::BindCollisionEvents()
{
	UnbindCollisionEvents();
	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	for (UActorComponent* Component : Owner->GetComponents())
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive)
			continue;

		FCollisionBinding Binding;
		Binding.Component = Primitive;
		Binding.OverlapHandle = Primitive->OnComponentBeginOverlap.AddRaw(this, &UScriptComponent::HandleBeginOverlap);
		Binding.HitHandle = Primitive->OnComponentHit.AddRaw(this, &UScriptComponent::HandleHit);
		CollisionBindings.Add(Binding);
	}
}

void UScriptComponent::UnbindCollisionEvents()
{
	for (FCollisionBinding& Binding : CollisionBindings)
	{
		if (!Binding.Component)
			continue;
		Binding.Component->OnComponentBeginOverlap.Remove(Binding.OverlapHandle);
		Binding.Component->OnComponentHit.Remove(Binding.HitHandle);
	}
	CollisionBindings.Reset();
}

void UScriptComponent::HandleBeginOverlap(UPrimitiveComponent* Self, AActor* OtherActor, UPrimitiveComponent* Other,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (OtherActor)
		OnOverlap(OtherActor);
}

void UScriptComponent::HandleHit(UPrimitiveComponent* Self, AActor* OtherActor, UPrimitiveComponent* Other,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor)
		OnHit(OtherActor, Hit.Normal);
}

void UScriptComponent::SetScriptPath(FString InScriptPath)
{
	ScriptPath = InScriptPath;
	bScriptNotFound = false;
}

FString UScriptComponent::GetScriptPath()
{
	return (ScriptPath);
}

const FStringView UScriptComponent::GetTemplateScriptPath()
{
	return (TemplatePath);
}

const FStringView UScriptComponent::GetScriptFolderPath()
{
	return (ScriptFolderPath);
}

template<typename... Args>
void UScriptComponent::ResumeRecord(FCoroutineRecord& Record, Args&&... InArgs)
{
	if (Record.State != ECoroutineState::Waiting)
		return;

	Record.LastFrame = FrameCounter;
	Record.State = ECoroutineState::Running;
	sol::protected_function_result Result = Record.Coroutine(std::forward<Args>(InArgs)...);

	switch (Result.status())
	{
	case sol::call_status::ok:
		if (Record.State == ECoroutineState::Running)
			Record.State = ECoroutineState::Finished;
		break;
	case sol::call_status::yielded:
		if (Record.State == ECoroutineState::Running)
			HandleYieldedResult(Result, Record);
		break;
	default:
	{ 
		sol::error Err = Result;
		Record.State = ECoroutineState::Errored;
		HTR_LOG(Error, "[ScriptComponent] ResumeRecord failed: {}", Err.what());
		break;
	}
	}
}

sol::optional<uint32> UScriptComponent::StartCoroutine(sol::protected_function Fn, sol::variadic_args Args)
{
	if (!bAcceptCoroutines)
	{
		HTR_LOG(Warning, "[Script] {}: StartCoroutine refused (component is not playing)", ScriptPath);
		return sol::nullopt;
	}

	sol::state& Lua = FScriptSystem::Get().GetLua();

	TUniquePtr<FCoroutineRecord> NewRecord = MakeUnique<FCoroutineRecord>();
	NewRecord->Id = NextCoroutineId++;
	NewRecord->Thread = sol::thread::create(Lua.lua_state());
	NewRecord->Coroutine = sol::coroutine(NewRecord->Thread.thread_state(), Fn);

	FCoroutineRecord* Record = NewRecord.get();
	const uint32 Id = Record->Id;
	CoroutineRecords.Add(std::move(NewRecord));
	ResumeRecord(*Record, Args);
	return Id;
}

void UScriptComponent::HandleYieldedResult(sol::protected_function_result& Result, FCoroutineRecord& Record)
{
	int ReturnCount = Result.return_count();
	if (ReturnCount == 0)
	{
		Record.WaitType = EWaitType::None;
		Record.State = ECoroutineState::Waiting;
		return;
	}
	
	sol::object Value = Result.get<sol::object>();
	sol::type Type = Value.get_type();
	if (Type == sol::type::lua_nil)
	{
		Record.WaitType = EWaitType::None;
		Record.State = ECoroutineState::Waiting;
		return;
	}
	else if (Type == sol::type::table)
	{
		sol::table Wait = Value.as<sol::table>();
		auto WaitType = Wait.get<sol::optional<std::string>>("WaitType");

		if (!WaitType)
		{
			Record.State = ECoroutineState::Errored;
			HTR_LOG(Error, "[Script] {}: WaitType is not string!", ScriptPath);
			return;
		}

		if (WaitType == "seconds")
		{
			auto Seconds = Wait.get<sol::optional<float>>("Seconds");

			if (!Seconds)
			{
				Record.State = ECoroutineState::Errored;
				HTR_LOG(Error, "[Script] {}: seconds not number!", ScriptPath);
				return;
			}

			Record.Remaining = *Seconds;
			Record.WaitType = EWaitType::Seconds;
			Record.State = ECoroutineState::Waiting;
			return;
		}
		else if (WaitType == "until" || WaitType == "while")
		{
			auto Predicate = Wait.get<sol::object>("Predicate");

			if (Predicate.get_type() != sol::type::function)
			{
				Record.State = ECoroutineState::Errored;
				HTR_LOG(Error, "[Script] {}: Predicate is not function", ScriptPath);
				return;
			}
			Record.Predicate = sol::protected_function(FScriptSystem::Get().GetLua().lua_state(), Predicate);
			Record.WaitType = WaitType == "until" ? EWaitType::Until : EWaitType::While;
			Record.State = ECoroutineState::Waiting;
			return;
		}
		else
		{
			Record.State = ECoroutineState::Errored;
			HTR_LOG(Error, "[Script] {}: Unknown WaitType {}", ScriptPath, *WaitType);
			return;
		}
	}
	else
	{
		Record.State = ECoroutineState::Errored;
		HTR_LOG(Error, "[Script] {}: Unknown yield value", ScriptPath);
		return;
	}
}

void UScriptComponent::UpdateCoroutines(float DeltaTime)
{
	for (int32 i = 0; i < CoroutineRecords.Num(); ++i)
	{
		FCoroutineRecord* Record = CoroutineRecords[i].get();
		if (Record->IsActive() && Record->LastFrame != FrameCounter && IsRecordReady(*Record, DeltaTime))
		{
			ResumeRecord(*Record);
		}
	}

	for (int32 i = CoroutineRecords.Num() - 1; i >= 0; --i)
	{
		if (!CoroutineRecords[i].get()->IsActive())
			CoroutineRecords.RemoveAt(static_cast<uint32>(i), 1);
	}
}

bool UScriptComponent::IsRecordReady(FCoroutineRecord& Record, float DeltaTime)
{
	EWaitType WaitType = Record.WaitType;

	switch (WaitType)
	{
	case EWaitType::None:
		return true;
	case EWaitType::Seconds:
		Record.Remaining -= DeltaTime;
		return Record.Remaining <= 0;
	case EWaitType::While:
	case EWaitType::Until:
	{
		sol::protected_function_result Result = Record.Predicate();

		if (!Result.valid())
		{
			sol::error Err = Result;
			Record.State = ECoroutineState::Errored;
			HTR_LOG(Error, "[Script] {}: wait condition failed (Predicate): {}", ScriptPath, Err.what());
			return false;
		}

		const bool bCondition = Result.get<bool>();
		return (WaitType == EWaitType::Until) ? bCondition : !bCondition;
	}
	}
	return false;
}

void UScriptComponent::StopRecord(FCoroutineRecord& Record)
{
	if (Record.IsActive())
		Record.State = ECoroutineState::Stopped;
}

void UScriptComponent::StopAllRecords()
{
	for (TUniquePtr<FCoroutineRecord>& Record : CoroutineRecords)
	{
		StopRecord(*Record);
	}
}

FCoroutineRecord* UScriptComponent::FindRecord(uint32 Id)
{
	for (TUniquePtr<FCoroutineRecord>& Record : CoroutineRecords)
	{
		if (Record->Id == Id)
			return Record.get();
	}

	return nullptr;
}