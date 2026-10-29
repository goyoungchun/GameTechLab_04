#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>
#include <wrl/client.h>
#include <filesystem>
#include "Container/Array.h"
#include "Container/Map.h"
#include "Core/EngineString.h"

class FAudioSystem
{
public:
	FAudioSystem(const FAudioSystem&) = delete;
	FAudioSystem& operator=(const FAudioSystem&) = delete;

	static FAudioSystem& Get();

	bool Initialize(HWND InWindow);
	void Shutdown();

	bool Play(const FString& Name);
	bool PlayLoop(const FString& Name);
	void Stop(const FString& Name);
	void StopAll();
	void SetVolume(const FString& Name, float Volume);
	void SetPitch(const FString& Name, float Pitch);

private:
	struct FSound
	{
		TArray<Microsoft::WRL::ComPtr<IDirectSoundBuffer>> Voices;
		LONG Volume = DSBVOLUME_MAX;
		DWORD BaseFrequency = 0;
		DWORD Frequency = 0;
		int32 NextVoice = 0;
	};

	FAudioSystem() = default;
	~FAudioSystem() = default;

	void LoadAllSounds();
	bool LoadSound(const FString& Name, const std::filesystem::path& Path);
	FSound* FindSound(const FString& Name);
	IDirectSoundBuffer* AcquireVoice(FSound& Sound);
	bool StartVoice(IDirectSoundBuffer* Voice, DWORD Flags);

	Microsoft::WRL::ComPtr<IDirectSound8> Device;
	TMap<FString, FSound> Sounds;

	static constexpr int32 MaxVoicesPerSound = 8;
};
