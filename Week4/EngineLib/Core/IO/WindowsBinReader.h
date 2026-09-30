#pragma once

#include <filesystem>
#include <fstream>

#include "Archive.h"


class FWindowsBinReader final : public FArchive
{
public:
	explicit FWindowsBinReader(const std::filesystem::path& filePath);

	void Serialize(void* data, uint64 size) override;

	uint64 RemainingBytes() const override
	{
		return mRemainingBytes;
	}


private:
	std::ifstream mFileStream;
	uint64 mRemainingBytes = 0;
};
