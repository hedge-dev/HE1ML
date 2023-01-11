#pragma once
#include <unordered_map>
#include <optional>
#include <mutex>

enum EBindError
{
	eBindError_None,
	eBindError_NotFound,
};

class FileBinder
{
	mutable std::unordered_map<size_t, std::optional<std::string>> lookup_cache;
	mutable std::mutex mtx{};
public:
	std::vector<std::string> bound_directories;

	void BindDirectory(const char* path);
	EBindError FileExists(const char* path) const;
	EBindError ResolvePath(const char* path, const char*& out) const;
};