#include "Pch.h"
#include "FileBinder.h"
#include "xxhash.h"


size_t FileBinder::Binding::AddDirectory(const std::string& path)
{
	if (std::holds_alternative<std::monostate>(value))
	{
		this->value = std::list<std::filesystem::path>{ path };
	}
	else if (std::holds_alternative<std::filesystem::path>(value))
	{
		this->value = std::list<std::filesystem::path>{ std::get<std::filesystem::path>(value), path };
	}
	else if (std::holds_alternative<std::list<std::filesystem::path>>(value))
	{
		auto& dirs = std::get<std::list<std::filesystem::path>>(value);
		dirs.emplace(dirs.begin(), path);
	}

	return std::get<std::list<std::filesystem::path>>(value).size() - 1;
}

void FileBinder::Binding::SetFile(const std::string& path)
{
	value = std::filesystem::path{ path };
}

bool FileBinder::Binding::Query(const char* file, std::string& out_path) const
{
	if (std::holds_alternative<std::monostate>(value))
	{
		return false;
	}
	else if (std::holds_alternative<std::filesystem::path>(value))
	{
		out_path = std::get<std::filesystem::path>(value).string();
		return true;
	}
	else if (std::holds_alternative<std::list<std::filesystem::path>>(value))
	{
		for (const auto& dir : std::get<std::list<std::filesystem::path>>(value))
		{
			const auto path = dir / file;
			auto pathString = path.string();
			const auto attributes = GetFileAttributesA(pathString.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				out_path = std::move(pathString);
				return true;
			}
		}
	}

	return false;
}

FileBinder::FileBinder()
{
	bindings.reserve(0x100);

	// Reserve the first binding
	bindings.emplace_back().emplace();
}

EBindError FileBinder::Unbind(size_t id)
{
	std::lock_guard lock(mtx);
	if (id == 0 || id >= bindings.size())
	{
		return eBindError_BadArguments;
	}

	bindings[id].reset();
	lookup_cache.clear();

	return eBindError_None;
}

EBindError FileBinder::EnumerateFiles(const char* path, const std::function<bool(const std::filesystem::path&)>& callback) const
{
	if (callback == nullptr)
	{
		return eBindError_BadArguments;
	}

	const auto* entry = vfs.get_entry(path);
	if (entry == nullptr || !entry->has_data<size_t>())
	{
		return eBindError_NotFound;
	}

	const auto& binding = bindings[entry->get_data<size_t>()].value();
	if (!std::holds_alternative<std::list<std::filesystem::path>>(binding.value))
	{
		return eBindError_NotFound;
	}

	const auto& folders = std::get<std::list<std::filesystem::path>>(binding.value);

	for (const auto& folder : folders)
	{
		for (const auto& file : std::filesystem::directory_iterator(folder))
		{
			if (file.is_directory())
			{
				continue;
			}

			if (!callback(file.path()))
			{
				break;
			}
		}
	}

	return eBindError_None;
}

EBindError FileBinder::BindFile(const char* path, const char* destination, size_t* out_id)
{
	std::lock_guard lock(mtx);
	if (path == nullptr)
	{
		return eBindError_BadArguments;
	}

	const auto attributes = GetFileAttributesA(destination);
	if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		lookup_cache.clear();

		auto* entry = vfs.get_entry(path);
		if (entry == nullptr)
		{
			entry = vfs.make_entry(path);
		}

		size_t bindId = 0;
		if (entry->has_data<size_t>())
		{
			bindId = entry->get_data<size_t>();
		}

		// Re-use binding, if any
		if (bindId != 0)
		{
			bindings[bindId].emplace().SetFile(destination);
		}
		else
		{
			bindId = entry->userdata.emplace<size_t>(AllocateBinding());
			bindings[bindId]->SetFile(destination);
		}

		if (out_id != nullptr)
		{
			*out_id = bindId;
		}

		return eBindError_None;
	}

	return eBindError_NotFound;
}

EBindError FileBinder::BindDirectory(const char* path, const char* destination)
{
	std::lock_guard lock(mtx);
	if (path == nullptr)
	{
		return eBindError_BadArguments;
	}

	// Add a trailing slash to path
	std::string pathStr = path;
	if (pathStr.back() != '/' && pathStr.back() != '\\')
	{
		pathStr += '\\';
	}

	const auto attributes = GetFileAttributesA(destination);
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return eBindError_NotFound;
	}

	lookup_cache.clear();
	auto* entry = vfs.make_entry(pathStr);
	size_t bindId = 0;
	if (entry->has_data<size_t>())
	{
		bindId = entry->get_data<size_t>();
	}

	if (bindId != 0)
	{
		bindings[bindId]->AddDirectory(destination);
	}
	else
	{
		bindId = entry->userdata.emplace<size_t>(AllocateBinding());
		bindings[bindId]->AddDirectory(destination);
	}

	return eBindError_None;
}

EBindError FileBinder::BindDirectoryRecursive(const char* path, const char* destination)
{
	const auto result = BindDirectory(path, destination);
	if (result != eBindError_None)
	{
		return result;
	}

	for (const auto& file : std::filesystem::recursive_directory_iterator(destination))
	{
		if (!file.is_directory())
		{
			continue;
		}

		const auto relativePath = std::filesystem::relative(file, destination).string();
		const auto fullPath = std::string(path) + '\\' + relativePath;
		BindDirectory(fullPath.c_str(), file.path().string().c_str());
	}

	return eBindError_None;
}

EBindError FileBinder::FileExists(const char* path) const
{
	return ResolvePath(path, nullptr);
}

EBindError FileBinder::ResolvePath(const char* path, std::string* out) const
{
	std::lock_guard lock(mtx);
	const size_t hash = XXH32(path, strlen(path), 0);
	const auto& it = lookup_cache.find(hash);
	if (it != lookup_cache.end())
	{
		if (out != nullptr)
		{
			*out = it->second.has_value() ? it->second.value().c_str() : path;
		}
		return it->second.has_value() ? eBindError_None : eBindError_NotFound;
	}

	EBindError error{ eBindError_NotFound };
	std::string fsPath{};
	vfs.root->walk(path, [&](VirtualFileSystem::Entry* entry) -> bool
		{
			if (!entry->has_data<size_t>())
			{
				return true;
			}

			const auto& binding = bindings[entry->get_data<size_t>()];
			if ((entry->is_root() || entry->parent->is_root()) && binding->Query(path, fsPath))
			{
				if (out != nullptr)
				{
					*out = fsPath;
				}
				error = eBindError_None;
				return false;
			}
			else if (!entry->is_root())
			{
				const auto rel = std::filesystem::relative(path, entry->full_path());
				if (binding->Query(rel.string().c_str(), fsPath))
				{
					if (out != nullptr)
					{
						*out = fsPath;
					}
					error = eBindError_None;
					return false;
				}
			}

			return true;
		}, VFS_ITERATE_RESOLVE_DIR_LINK);


	if (error == eBindError_NotFound)
	{
		lookup_cache.emplace(hash, std::nullopt);
	}
	else
	{
		lookup_cache.emplace(hash, fsPath);
	}

	return error;
}

size_t FileBinder::AllocateBinding()
{
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		if (!bindings[i].has_value())
		{
			bindings[i].emplace();
			return i;
		}
	}

	bindings.emplace_back().emplace();
	return bindings.size() - 1;
}

FileBinder::Binding& FileBinder::GetFreeBinding()
{
	return bindings[AllocateBinding()].value();
}