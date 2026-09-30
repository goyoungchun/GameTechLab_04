#pragma once

#include "../EngineLib/Core/Core.h"

enum class EArchiveMode : uint8
{
	Loading,
	Saving
};

class FArchive
{
public:
	virtual ~FArchive() = default;
	virtual void Serialize(void* data, size_t size) = 0;

	bool IsLoading() const { return mMode == EArchiveMode::Loading; }
	bool IsSaving() const { return mMode == EArchiveMode::Saving; }
	bool HasError() const { return bHasError; }
	void SetError() { bHasError = true; }

	//남은 바이트 수를 반환한다.
	virtual uint64 RemainingBytes() const
	{
		return (std::numeric_limits<uint64>::max)();
	}

protected:
	explicit FArchive(EArchiveMode mode) : mMode(mode) {}

private:
	EArchiveMode mMode;
	bool bHasError = false;
};

template<typename T>
requires std::is_arithmetic_v<T>
FArchive& operator<<(FArchive& ar, T& value)
{
	if(ar.HasError())
	{
		return ar;
	}
	ar.Serialize(&value, sizeof(T));
	return ar;
}

inline FArchive& operator<<(FArchive& archive, FString & value)
{
	if (archive.HasError())
	{
		return archive;
	}

	constexpr uint32 MaxStringByte = 1024 * 1024 * 64; // 64MB

	uint32 length = archive.IsSaving() ? static_cast<uint32>(value.Len()) : 0;

	archive << length;

	if (length > MaxStringByte || (archive.IsLoading() && length > archive.RemainingBytes()))
	{
		archive.SetError();
		return archive;
	}

	if (archive.IsSaving())
	{
		if (length > 0)
		{
			archive.Serialize(const_cast<char*>(value.CStr()), length);
		}

		return archive;
	}

	std::string buffer(length, '\0');

	if (length > 0)
	{
		archive.Serialize( buffer.data(), length);
	}

	if (!archive.HasError())
	{
		value = std::string_view(
			buffer.data(),
			buffer.size());
	}

	return archive;

}

