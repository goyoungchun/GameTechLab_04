#include "EnginePCH.h"
#include "Audio/AudioSystem.h"

#include <mmsystem.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <vector>

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
	constexpr const char* SoundDirectory = "Assets/Sound";

	bool ReadWaveFile(const std::filesystem::path& Path, std::vector<char>& OutFormat, std::vector<char>& OutData)
	{
		std::wstring WidePath = Path.wstring();
		HMMIO File = mmioOpenW(WidePath.data(), nullptr, MMIO_READ | MMIO_ALLOCBUF);
		if (!File)
		{
			return false;
		}

		MMCKINFO Riff{};
		Riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		bool bSucceeded = mmioDescend(File, &Riff, nullptr, MMIO_FINDRIFF) == MMSYSERR_NOERROR;

		MMCKINFO Chunk{};
		if (bSucceeded)
		{
			Chunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
			bSucceeded = mmioDescend(File, &Chunk, &Riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR;
		}
		if (bSucceeded)
		{
			OutFormat.assign(std::max<size_t>(Chunk.cksize, sizeof(WAVEFORMATEX)), 0);
			const LONG FormatSize = static_cast<LONG>(Chunk.cksize);
			bSucceeded = mmioRead(File, OutFormat.data(), FormatSize) == FormatSize;
			mmioAscend(File, &Chunk, 0);
		}
		if (bSucceeded)
		{
			Chunk = {};
			Chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
			bSucceeded = mmioDescend(File, &Chunk, &Riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR && Chunk.cksize > 0;
		}
		if (bSucceeded)
		{
			OutData.resize(Chunk.cksize);
			const LONG DataSize = static_cast<LONG>(Chunk.cksize);
			bSucceeded = mmioRead(File, OutData.data(), DataSize) == DataSize;
		}

		mmioClose(File, 0);
		return bSucceeded;
	}

	LONG ToDirectSoundVolume(float Volume)
	{
		const float Clamped = std::clamp(Volume, 0.0f, 1.0f);
		if (Clamped <= 0.0001f)
		{
			return DSBVOLUME_MIN;
		}
		const LONG Attenuation = static_cast<LONG>(2000.0f * std::log10(Clamped));
		return std::clamp<LONG>(Attenuation, DSBVOLUME_MIN, DSBVOLUME_MAX);
	}
}

FAudioSystem& FAudioSystem::Get()
{
	static FAudioSystem Instance;
	return Instance;
}

bool FAudioSystem::Initialize(HWND InWindow)
{
	if (Device)
	{
		return true;
	}

	if (FAILED(DirectSoundCreate8(nullptr, Device.GetAddressOf(), nullptr)))
	{
		HTR_LOG(Warning, "Audio: Failed to create DirectSound");
		return false;
	}
	if (FAILED(Device->SetCooperativeLevel(InWindow, DSSCL_PRIORITY)))
	{
		HTR_LOG(Warning, "Audio: Failed to set cooperative level");
		Device.Reset();
		return false;
	}

	LoadAllSounds();
	return true;
}

void FAudioSystem::Shutdown()
{
	StopAll();
	Sounds.Reset();
	Device.Reset();
}

void FAudioSystem::LoadAllSounds()
{
	std::error_code Error;
	if (!std::filesystem::is_directory(SoundDirectory, Error))
	{
		HTR_LOG(Warning, "Audio: Sound directory {} not found", SoundDirectory);
		return;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(SoundDirectory, Error))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		FString Extension = Entry.path().extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		if (Extension != ".wav")
		{
			continue;
		}

		const FString Name = Entry.path().stem().string();
		if (!LoadSound(Name, Entry.path()))
		{
			HTR_LOG(Warning, "Audio: Failed to load {}", Entry.path().string());
		}
	}
}

bool FAudioSystem::LoadSound(const FString& Name, const std::filesystem::path& Path)
{
	std::vector<char> Format;
	std::vector<char> Data;
	if (!Device || !ReadWaveFile(Path, Format, Data))
	{
		return false;
	}

	DSBUFFERDESC Desc{};
	Desc.dwSize = sizeof(DSBUFFERDESC);
	Desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
	Desc.dwBufferBytes = static_cast<DWORD>(Data.size());
	Desc.lpwfxFormat = reinterpret_cast<WAVEFORMATEX*>(Format.data());

	Microsoft::WRL::ComPtr<IDirectSoundBuffer> Buffer;
	if (FAILED(Device->CreateSoundBuffer(&Desc, Buffer.GetAddressOf(), nullptr)))
	{
		return false;
	}

	void* LockedData = nullptr;
	DWORD LockedSize = 0;
	if (FAILED(Buffer->Lock(0, 0, &LockedData, &LockedSize, nullptr, nullptr, DSBLOCK_ENTIREBUFFER)))
	{
		return false;
	}
	std::memcpy(LockedData, Data.data(), std::min<size_t>(LockedSize, Data.size()));
	Buffer->Unlock(LockedData, LockedSize, nullptr, 0);

	FSound Sound;
	Sound.BaseFrequency = reinterpret_cast<WAVEFORMATEX*>(Format.data())->nSamplesPerSec;
	Sound.Frequency = Sound.BaseFrequency;
	Sound.Voices.Add(Buffer);
	Sounds.Add(Name, Sound);
	return true;
}

FAudioSystem::FSound* FAudioSystem::FindSound(const FString& Name)
{
	FSound* Sound = Sounds.Find(Name);
	if (!Sound || Sound->Voices.IsEmpty())
	{
		HTR_LOG(Warning, "Audio: Sound {} not found", Name);
		return nullptr;
	}
	return Sound;
}

IDirectSoundBuffer* FAudioSystem::AcquireVoice(FSound& Sound)
{
	for (const Microsoft::WRL::ComPtr<IDirectSoundBuffer>& Voice : Sound.Voices)
	{
		DWORD Status = 0;
		if (SUCCEEDED(Voice->GetStatus(&Status)) && !(Status & DSBSTATUS_PLAYING))
		{
			return Voice.Get();
		}
	}

	if (Sound.Voices.Num() < MaxVoicesPerSound)
	{
		Microsoft::WRL::ComPtr<IDirectSoundBuffer> Duplicate;
		if (SUCCEEDED(Device->DuplicateSoundBuffer(Sound.Voices[0].Get(), Duplicate.GetAddressOf())))
		{
			Duplicate->SetVolume(Sound.Volume);
			Duplicate->SetFrequency(Sound.Frequency);
			Sound.Voices.Add(Duplicate);
			return Duplicate.Get();
		}
	}

	IDirectSoundBuffer* Oldest = Sound.Voices[Sound.NextVoice % Sound.Voices.Num()].Get();
	Sound.NextVoice = (Sound.NextVoice + 1) % Sound.Voices.Num();
	return Oldest;
}

bool FAudioSystem::StartVoice(IDirectSoundBuffer* Voice, DWORD Flags)
{
	if (!Voice)
	{
		return false;
	}

	Voice->Stop();
	Voice->SetCurrentPosition(0);
	return SUCCEEDED(Voice->Play(0, 0, Flags));
}

bool FAudioSystem::Play(const FString& Name)
{
	FSound* Sound = FindSound(Name);
	return Sound && StartVoice(AcquireVoice(*Sound), 0);
}

bool FAudioSystem::PlayLoop(const FString& Name)
{
	FSound* Sound = FindSound(Name);
	return Sound && StartVoice(Sound->Voices[0].Get(), DSBPLAY_LOOPING);
}

void FAudioSystem::Stop(const FString& Name)
{
	if (FSound* Sound = FindSound(Name))
	{
		for (const Microsoft::WRL::ComPtr<IDirectSoundBuffer>& Voice : Sound->Voices)
		{
			Voice->Stop();
			Voice->SetCurrentPosition(0);
		}
	}
}

void FAudioSystem::StopAll()
{
	for (auto& Pair : Sounds)
	{
		for (const Microsoft::WRL::ComPtr<IDirectSoundBuffer>& Voice : Pair.second.Voices)
		{
			Voice->Stop();
			Voice->SetCurrentPosition(0);
		}
	}
}

void FAudioSystem::SetVolume(const FString& Name, float Volume)
{
	if (FSound* Sound = FindSound(Name))
	{
		Sound->Volume = ToDirectSoundVolume(Volume);
		for (const Microsoft::WRL::ComPtr<IDirectSoundBuffer>& Voice : Sound->Voices)
		{
			Voice->SetVolume(Sound->Volume);
		}
	}
}

void FAudioSystem::SetPitch(const FString& Name, float Pitch)
{
	if (FSound* Sound = FindSound(Name))
	{
		const float Frequency = static_cast<float>(Sound->BaseFrequency) * std::max(Pitch, 0.0f);
		Sound->Frequency = static_cast<DWORD>(std::clamp(Frequency, static_cast<float>(DSBFREQUENCY_MIN), static_cast<float>(DSBFREQUENCY_MAX)));
		for (const Microsoft::WRL::ComPtr<IDirectSoundBuffer>& Voice : Sound->Voices)
		{
			Voice->SetFrequency(Sound->Frequency);
		}
	}
}
