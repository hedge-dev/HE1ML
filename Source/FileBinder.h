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
	mutable std::unordered_map<size_t, std::optional<std::string>> lookup_cache;
	mutable std::mutex mtx{};

public:
	struct Binding
	{
		struct Bind
		{
			int priority{};
			std::filesystem::path path{};

			bool operator<(const Bind& other) const
			{
				return priority < other.priority;
			}

			bool operator>(const Bind& other) const
			{
				return priority > other.priority;
			}
		};

		priority_queue<Bind> binds;
		bool is_directory{};
		void BindDirectory(const std::string& path, int priority);
		void BindFile(const std::string& path, int priority);

		const Bind* Query(const char* file, std::string& out_path) const;
		void Query(const char* file, const std::function<bool(const std::string_view&, const Bind&)>& callback) const;
		int HighestPriority() const
		{
			if (binds.empty())
			{
				return INT_MIN;
			}
			return binds.begin()->priority;
		}
	};

	struct BindResult
	{
		const Binding::Bind& bind;
		std::string path;

		operator const Binding::Bind&() const
		{
			return bind;
		}

		bool operator<(const BindResult& other) const
		{
			return bind < other.bind;
		}

		bool operator>(const BindResult& other) const
		{
			return bind > other.bind;
		}
	};

	std::vector<std::optional<Binding>> bindings{};
	VirtualFileSystem vfs{};

	FileBinder();
	EBindError Unbind(size_t id);
	EBindError EnumerateFiles(const char* path, const std::function<bool(const std::filesystem::path&)>& callback) const;
	EBindError BindFile(const char* path, const char* destination, int priority);
	EBindError BindDirectory(const char* path, const char* destination, int priority);
	EBindError FileExists(const char* path) const;
	EBindError ResolvePath(const char* path, std::string* out) const;
	Binding& GetFreeBinding();
	size_t AllocateBinding();

	priority_queue<BindResult, std::greater<const Binding::Bind>> CollectBindings(const char* path) const;
};