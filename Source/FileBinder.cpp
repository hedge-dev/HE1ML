#include "Pch.h"
#include "FileBinder.h"
#include "xxhash.h"

void FileBinder::BindDirectory(const char* path)
{
	std::lock_guard lock(mtx);
	if (path == nullptr)
	{
		return;
	}

	// Add a trailing slash to path
	std::string pathStr = path;
	if (pathStr.back() != '/' && pathStr.back() != '\\')
	{
		pathStr += '\\';
	}

	if (!lookup_cache.empty())
		lookup_cache.clear();
	bound_directories.push_back(pathStr);
}

EBindError FileBinder::FileExists(const char* path) const
{
	std::lock_guard lock(mtx);
	const size_t hash = XXH32(path, strlen(path), 0);
	const auto& it = lookup_cache.find(hash);
	if (it != lookup_cache.end())
	{
		return it->second.has_value() ? eBindError_None : eBindError_NotFound;
	}

	for (const auto& dir : bound_directories)
	{
		std::string fullPath = dir + path;
		if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			lookup_cache.insert_or_assign(hash, fullPath);
			return eBindError_None;
		}
	}

	lookup_cache.emplace(hash, std::nullopt);
	return eBindError_NotFound;
}

EBindError FileBinder::ResolvePath(const char* path, const char*& out) const
{
	std::lock_guard lock(mtx);
	const size_t hash = XXH32(path, strlen(path), 0);
	const auto& it = lookup_cache.find(hash);
	if (it != lookup_cache.end())
	{
		out = it->second.has_value() ? it->second.value().c_str() : path;
		return it->second.has_value() ? eBindError_None : eBindError_NotFound;
	}

	for (const auto& dir : bound_directories)
	{
		std::string fullPath = dir + path;
		if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			out = lookup_cache.insert_or_assign(hash, fullPath).first->second->c_str();
			return eBindError_None;
		}
	}

	lookup_cache.emplace(hash, std::nullopt);
	return eBindError_NotFound;
}
