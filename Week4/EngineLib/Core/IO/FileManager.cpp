#include <fstream>
#include <sstream>
#include <algorithm>

#include "FileManager.h"

FFileManager::FFileManager()
	: FFileManager(kDefaultAssetsPath, kDefaultRootPath)
{
}

FFileManager::FFileManager(std::string_view fileDirPath)
	: FFileManager(fileDirPath, kDefaultRootPath)
{
}

FFileManager::FFileManager(std::string_view fileDirPath, std::string_view rootPath)
	: mFileDirPath(fileDirPath)
	, mRootPath(rootPath)
{
}

FString FFileManager::ReadFileToString(std::string_view filePath) const
{
	std::filesystem::path path(filePath);

	std::ifstream fileStream(path, std::ios::in);
	if (!fileStream.is_open())
	{
		throw std::runtime_error("Failed to open file for reading: " + path.string());
	}

	std::stringstream buffer;
	buffer << fileStream.rdbuf();

	return FString(buffer.str());
}

void FFileManager::WriteStringToFile(std::string_view filePath, std::string_view content) const
{
	std::filesystem::path path(filePath);

	std::ofstream fileStream(path, std::ios::out);
	if (!fileStream.is_open())
	{
		throw std::runtime_error("Failed to open file for writing: " + path.string());
	}

	fileStream << content;
	return;
}

bool FFileManager::IsUnderRoot(const std::filesystem::path& filePath) const
{
	return IsUnder(filePath, mRootPath);
}

bool FFileManager::IsUnderFileDir(const std::filesystem::path& filePath) const
{
	return IsUnder(filePath, mFileDirPath);
}

bool IsUnder(const std::filesystem::path& targetPath, const std::filesystem::path& basePath)
{
	std::error_code error;
	auto normalizedTarget = std::filesystem::weakly_canonical(targetPath, error);
	if (error)
	{
		error.clear();
		normalizedTarget = std::filesystem::absolute(targetPath, error).lexically_normal();
	}

	if (error)
	{
		return false;
	}

	auto normalizedBase = std::filesystem::weakly_canonical(basePath, error);
	if (error)
	{
		error.clear();
		normalizedBase = std::filesystem::absolute(basePath, error).lexically_normal();
	}

	if (error)
	{
		return false;
	}

	const std::filesystem::path relativePath =
		normalizedTarget.lexically_relative(normalizedBase);

	return !relativePath.empty() &&
		!relativePath.is_absolute() &&
		*relativePath.begin() != "..";
}
