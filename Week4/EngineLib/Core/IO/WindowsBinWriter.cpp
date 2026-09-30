#include "WindowsBinWriter.h"

#include <climits>

FWindowsBinWriter::FWindowsBinWriter(const std::filesystem::path& filePath)
	: FArchive(EArchiveMode::Saving)
	, mFileStream(filePath, std::ios::binary | std::ios::out | std::ios::trunc)
{
	if (!mFileStream.is_open())
	{
		SetError();
	}
}

void FWindowsBinWriter::Serialize(void* data, uint64 size)
{
	if (HasError() || size == 0) { return; }

	constexpr uint64 MaxWriteSize = std::numeric_limits<std::streamsize>::max();

	if (!data || size > MaxWriteSize)
	{
		SetError();
		return;
	}

	const auto requestedSize = static_cast<std::streamsize>(size);

	mFileStream.write(static_cast<const char*>(data), requestedSize);

	if (!mFileStream)
	{
		SetError();
	}
}

void FWindowsBinWriter::Flush()
{
	mFileStream.flush();

	if (!mFileStream){ SetError(); }
}
