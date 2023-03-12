#pragma once
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <mutex>
#include "VirtualFileSystem.h"

enum EBindError
{
	eBindError_None,
	eBindError_NotFound,
	eBindError_BadArguments,
};

class FileBinder
{
	std::unordered_set<std::string> strings;
	mutable std::unordered_map<size_t, std::optional<std::string>> lookup_cache;
	mutable std::mutex mtx{};
	VirtualFileSystem vfs{};

public:
	std::vector<std::string> bound_directories;

	EBindError UnbindFile(const char* path);
	EBindError BindFile(const char* path, const char* destination);
	EBindError BindDirectory(const char* path);
	EBindError FileExists(const char* path) const;
	EBindError ResolvePath(const char* path, const char*& out) const;
};