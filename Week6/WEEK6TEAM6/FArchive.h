#pragma once

#include "Core.h"
#include "TArray.h"
#include "Object.h"
#include <Windows.h>
#include <filesystem>
#include <sstream>

template <typename T>
concept CArchiveArithmetic = std::is_arithmetic_v<T> && !std::is_pointer_v<T>;

template <typename T>
struct FArchiveSerializer;

enum class EArchiveMode
{
	Read,
	Write
};

class FArchive
{
public:
	FArchive(EArchiveMode InMode) : Mode(InMode) {}
	virtual ~FArchive() = default;

	virtual uint64 Tell() const = 0;
	virtual uint64 TotalSize() const = 0;
	virtual bool Seek(uint64 NewPosition) = 0;

	virtual bool Serialize(void* Data, uint64 Size) = 0;

	// int, float, double 등 (enum 제외)
	template <typename T>
	requires (CArchiveArithmetic<T> && !std::is_enum_v<T>)
	FArchive& operator<<(T& Value)
	{
		Serialize(&Value, sizeof(T));
		return *this;
	}

	// enum 타입
	template <typename T>
	requires std::is_enum_v<T>
	FArchive& operator<<(T& Value)
	{
		using UnderlyingType = std::underlying_type_t<T>;

		UnderlyingType TempValue = static_cast<UnderlyingType>(Value);
		Serialize(&TempValue, sizeof(UnderlyingType));
		Value = static_cast<T>(TempValue);

		return *this;
	}

	// 그 외 사용자 정의 타입
	template<typename T>
	requires (!std::is_arithmetic_v<T> && !std::is_enum_v<T>)
	FArchive& operator<<(T& Value)
	{
		FArchiveSerializer<T>::Serialize(*this, Value);
		return *this;
	}

	// TODO: 나중에 추상 가상 함수로 승격시켜서 모든 Archive들이 직렬화/역직렬화가 가능하도록 구현. 현재는 필요한 경우에만 SerializeObject를 오버라이드해서 사용.
	virtual void SerializeObject(UObject*& Object) {};

	inline EArchiveMode GetMode() const { return Mode; }

private:
	EArchiveMode Mode;
};

// 커스텀 struct/class 직렬화 포인트를 위한 기본 템플릿
template <typename T>
struct FArchiveSerializer
{
	static void Serialize(FArchive& Ar, T& Value) 
	{
		static_assert(false, "Serializer not implemented for this type");
	}
};

// TArray<T> 전용 직렬화
template <typename T>
inline  FArchive& operator<<(FArchive& Ar, TArray<T>& Array)
{
	uint64 Count = Array.Num();
	if (Ar.GetMode() == EArchiveMode::Write)
	{
		Ar << Count;
		if (Count > 0)
		{
			if constexpr (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
			{
				Ar.Serialize(Array.Data(), Count * sizeof(T));
			}
			else
			{
				for (T& Element : Array)
				{
					Ar << Element;
				}
			}
		}
	}
	else if (Ar.GetMode() == EArchiveMode::Read)
	{
		Ar << Count;
		if (Count > 0)
		{
			Array.SetNum(static_cast<int32>(Count));

			if constexpr (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
			{
				Ar.Serialize(Array.Data(), Count * sizeof(T));
			}
			else
			{
				for (T& Element : Array)
				{
					Ar << Element;
				}
			}
		}
	}

	return Ar;
}

// 바이너리 파일 쓰기 아카이브
class FWindowsBinWriter : public FArchive
{
public:
	FWindowsBinWriter(const std::filesystem::path& InFilePath)
		: FArchive(EArchiveMode::Write)
	{
		FileHandle = CreateFileW(
			InFilePath.c_str(),
			GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (FileHandle == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Failed to open file for writing: " + InFilePath.string());
		}
	}

	~FWindowsBinWriter()
	{
		if (FileHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(FileHandle);
		}
	}

	uint64 Tell() const override
	{
		LARGE_INTEGER CurrentPosition;
		SetFilePointerEx(FileHandle, { 0 }, &CurrentPosition, FILE_CURRENT);
		return static_cast<uint64>(CurrentPosition.QuadPart);
	}

	uint64 TotalSize() const override
	{
		LARGE_INTEGER FileSize;
		GetFileSizeEx(FileHandle, &FileSize);
		return static_cast<uint64>(FileSize.QuadPart);
	}

	bool Seek(uint64 NewPosition) override
	{
		LARGE_INTEGER NewPos;
		NewPos.QuadPart = static_cast<LONGLONG>(NewPosition);
		return SetFilePointerEx(FileHandle, NewPos, NULL, FILE_BEGIN) != 0;
	}

	bool Serialize(void* Data, uint64 Size) override
	{
		const uint8* DataPtr = static_cast<const uint8*>(Data);

		uint64 RemainingSize = Size;
		while (RemainingSize > 0)
		{
			DWORD BytesWritten = 0;
			if (!WriteFile(FileHandle, DataPtr, static_cast<DWORD>(RemainingSize), &BytesWritten, NULL))
			{
				return false;
			}

			DataPtr += BytesWritten;
			RemainingSize -= BytesWritten;
		}

		return true;
	}

private:
	HANDLE FileHandle = INVALID_HANDLE_VALUE;
};

// 바이너리 파일 읽기 아카이브
class FWindowsBinReader : public FArchive
{
public:
	FWindowsBinReader(const std::filesystem::path& InFilePath)
		: FArchive(EArchiveMode::Read)
	{
		FileHandle = CreateFileW(
			InFilePath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (FileHandle == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Failed to open file for reading: " + InFilePath.string());
		}
	}

	~FWindowsBinReader()
	{
		if (FileHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(FileHandle);
		}
	}

	uint64 Tell() const override
	{
		LARGE_INTEGER CurrentPosition;
		SetFilePointerEx(FileHandle, { 0 }, &CurrentPosition, FILE_CURRENT);
		return static_cast<uint64>(CurrentPosition.QuadPart);
	}

	uint64 TotalSize() const override
	{
		LARGE_INTEGER FileSize;
		GetFileSizeEx(FileHandle, &FileSize);
		return static_cast<uint64>(FileSize.QuadPart);
	}

	bool Seek(uint64 NewPosition) override
	{
		LARGE_INTEGER NewPos;
		NewPos.QuadPart = static_cast<LONGLONG>(NewPosition);
		return SetFilePointerEx(FileHandle, NewPos, NULL, FILE_BEGIN) != 0;
	}

	bool Serialize(void* Data, uint64 Size) override
	{
		uint8* DataPtr = static_cast<uint8*>(Data);

		uint64 RemainingSize = Size;
		while (RemainingSize > 0)
		{
			DWORD BytesRead = 0;
			if (!ReadFile(FileHandle, DataPtr, static_cast<DWORD>(RemainingSize), &BytesRead, NULL))
			{
				return false;
			}
			if (BytesRead == 0)
			{
				return false;
			}

			DataPtr += BytesRead;
			RemainingSize -= BytesRead;
		}

		return true;
	}

private:
	HANDLE FileHandle = INVALID_HANDLE_VALUE;
};

class FMemoryWriter : public FArchive
{
public:
	FMemoryWriter()
		: FArchive(EArchiveMode::Write)
		, MemoryStream(std::ios::binary)
	{
	}

	~FMemoryWriter() = default;

	uint64 Tell() const override
	{
		auto Pos = MemoryStream.tellp();
		return static_cast<uint64>(Pos);
	}

	uint64 TotalSize() const override
	{
		auto CurrentPos = MemoryStream.tellp();
		MemoryStream.seekp(0, std::ios::end);
		auto TotalSize = MemoryStream.tellp();
		MemoryStream.seekp(CurrentPos, std::ios::beg);
		return static_cast<uint64>(TotalSize);
	}

	bool Seek(uint64 NewPosition) override
	{
		MemoryStream.seekp(static_cast<std::streamoff>(NewPosition), std::ios::beg);
		return true;
	}

	bool Serialize(void* Data, uint64 Size) override
	{
		MemoryStream.write(static_cast<const char*>(Data), static_cast<std::streamsize>(Size));
		return true;
	}

	TArray<int8> GetData() const
	{
		std::string str = MemoryStream.str();

		TArray<int8> Data;
		Data.SetNum(static_cast<int32>(str.size()));
		std::memcpy(Data.Data(), str.data(), str.size());

		return Data;
	}

private:
	mutable std::ostringstream MemoryStream;
};

class FMemoryReader : public FArchive
{
public:
	FMemoryReader(const TArray<int8>& InData)
		: FArchive(EArchiveMode::Read)
		, MemoryStream(std::string(reinterpret_cast<const char*>(InData.Data()), InData.Num()), std::ios::binary)
	{
	}

	~FMemoryReader() = default;

	uint64 Tell() const override
	{
		auto Pos = MemoryStream.tellg();
		return static_cast<uint64>(Pos);
	}

	uint64 TotalSize() const override
	{
		auto CurrentPos = MemoryStream.tellg();
		MemoryStream.seekg(0, std::ios::end);
		auto TotalSize = MemoryStream.tellg();
		MemoryStream.seekg(CurrentPos, std::ios::beg);
		return static_cast<uint64>(TotalSize);
	}

	bool Seek(uint64 NewPosition) override
	{
		MemoryStream.seekg(static_cast<std::streamoff>(NewPosition), std::ios::beg);
		return true;
	}

	bool Serialize(void* Data, uint64 Size) override
	{
		MemoryStream.read(static_cast<char*>(Data), static_cast<std::streamsize>(Size));
		return true;
	}

private:
	mutable std::istringstream MemoryStream;
};

inline bool TryReadToBytes(FArchive& Ar, TArray<int8>& OutBytes)
{
	if (Ar.GetMode() != EArchiveMode::Read)
	{
		// 아카이브가 읽기 모드가 아닌 경우, 읽을 수 없으므로 false를 반환합니다.
		return false;
	}
	
	uint64 CurrentPos = Ar.Tell();
	uint64 TotalSize = Ar.TotalSize();
	if (CurrentPos > TotalSize)
	{
		return false;
	}
	uint64 RemainingSize = TotalSize - CurrentPos;

	OutBytes.SetNum(static_cast<int32>(RemainingSize));

	return Ar.Serialize(OutBytes.Data(), RemainingSize);
}