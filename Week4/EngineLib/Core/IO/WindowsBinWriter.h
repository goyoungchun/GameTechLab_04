#pragma once

#include <filesystem>
#include <fstream>

#include "Archive.h"

class FWindowsBinWriter final : public FArchive
{
public:
	explicit FWindowsBinWriter(const std::filesystem::path& filePath);
	void Serialize(void* data, uint64 size) override;

	void Flush();
private:
	std::ofstream mFileStream;
};
