#pragma once
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <mutex>
#include <variant>
#include "VirtualFileSystem.h"

enum EBindError
{
	eBindError_None,
	eBindError_NotFound,
	eBindError_BadArguments,
};

class FileBinder
{
	mutable std::unordered_map<size_t, std::optional<std::string>> lookup_cache;
	mutable std::mutex mtx{};

public:
	struct Binding
	{
		std::variant<std::monostate, std::filesystem::path, std::vector<std::filesystem::path>> value;
		size_t AddDirectory(const std::string& path);
		void SetFile(const std::string& path);

		bool Query(const char* file, std::string& out_path) const;
	};

	std::vector<std::optional<Binding>> bindings{};
	VirtualFileSystem vfs{};

	FileBinder();
	EBindError Unbind(size_t id);
	EBindError BindFile(const char* path, const char* destination, size_t* out_id = nullptr);
	EBindError BindDirectory(const char* path, const char* destination);
	EBindError FileExists(const char* path) const;
	EBindError ResolvePath(const char* path, std::string* out) const;
	Binding& GetFreeBinding();
	size_t AllocateBinding();
};