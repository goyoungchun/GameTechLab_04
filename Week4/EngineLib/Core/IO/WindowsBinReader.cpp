#include "WindowsBinReader.h"

#include <limits>

FWindowsBinReader::FWindowsBinReader(const std::filesystem::path& filePath)
	: FArchive(EArchiveMode::Loading)
	, mFileStream(filePath, std::ios::binary | std::ios::in)
{
	if (!mFileStream.is_open())
	{
		SetError();
	}
	mFileStream.seekg(0, std::ios::end);
	const auto endPosition = mFileStream.tellg();

	if (endPosition == std::ifstream::pos_type(-1))
	{
		SetError();
		return;
	}
	mRemainingBytes = static_cast<uint64>(static_cast<std::streamoff>(endPosition));

	mFileStream.seekg(0, std::ios::beg);

	if (!mFileStream)
	{
		SetError();
	}
}

void FWindowsBinReader::Serialize(void* data, uint64 size)
{
	if (HasError() || size ==0 ){ return; }

	constexpr uint64 MaxReadSize = std::numeric_limits<std::streamsize>::max();

	if (!data || size > MaxReadSize || size > mRemainingBytes)
	{
		SetError();
		return;
	}

	const auto requestedSize = static_cast<std::streamsize>(size);

	mFileStream.read(static_cast<char*>(data), requestedSize);

	if (!mFileStream || mFileStream.gcount() != requestedSize)
	{
		SetError();
	}

	mRemainingBytes -= size;
}
