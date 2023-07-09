#include "Pch.h"
#include "FileBinder.h"
#include "xxhash.h"

void FileBinder::Binding::BindDirectory(const std::string& path, int priority)
{
	if (!is_directory)
	{
		binds.clear();
		is_directory = true;
	}

	binds.emplace(priority, path);
}

void FileBinder::Binding::BindFile(const std::string& path, int priority)
{
	if (is_directory)
	{
		binds.clear();
		is_directory = false;
	}

	binds.emplace(priority, path);
}

const FileBinder::Binding::Bind* FileBinder::Binding::Query(const char* file, std::string& out_path) const
{
	if (is_directory)
	{
		for (const auto& bind : binds)
		{
			const auto path = bind.path / file;
			auto pathString = path.string();
			const auto attributes = GetFileAttributesA(pathString.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				out_path = std::move(pathString);
				return &bind;
			}
		}
	}
	else
	{
		if (binds.empty())
		{
			return nullptr;
		}

		out_path = binds.begin()->path.string();
		return &*binds.begin();
	}

	return nullptr;
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
	if (std::filesystem::exists(path))
	{
		for (auto& filePath : std::filesystem::directory_iterator(path))
			callback(filePath);
	}

	//if (callback == nullptr)
	//{
	//	return eBindError_BadArguments;
	//}

	//const auto* entry = vfs.get_entry(path);
	//if (entry == nullptr || !entry->has_data<size_t>())
	//{
	//	return eBindError_NotFound;
	//}

	//const auto& binding = bindings[entry->get_data<size_t>()].value();
	//if (!std::holds_alternative<std::list<std::filesystem::path>>(binding.value))
	//{
	//	return eBindError_NotFound;
	//}

	//const auto& folders = std::get<std::list<std::filesystem::path>>(binding.value);

	//for (const auto& folder : folders)
	//{
	//	for (const auto& file : std::filesystem::directory_iterator(folder))
	//	{
	//		if (file.is_directory())
	//		{
	//			continue;
	//		}

	//		if (!callback(file.path()))
	//		{
	//			break;
	//		}
	//	}
	//}

	return eBindError_None;
}

EBindError FileBinder::BindFile(const char* path, const char* destination, int priority)
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

		//if (priority == 0)
		//{
		//	auto* node = entry;
		//	while (node != nullptr)
		//	{
		//		if (node->has_data<size_t>())
		//		{
		//			const auto& bind = bindings[node->get_data<size_t>()].value();
		//			for (const auto& b : bind.binds)
		//			{
		//				if (b.priority == priority)
		//				{
		//					priority = b.priority - 1;
		//				}
		//			}
		//		}

		//		node = node->parent;
		//	}
		//}

		size_t bindId = 0;
		if (entry->has_data<size_t>())
		{
			bindId = entry->get_data<size_t>();
		}

		// Re-use binding, if any
		if (bindId != 0)
		{
			bindings[bindId]->BindFile(destination, priority);
		}
		else
		{
			bindId = entry->userdata.emplace<size_t>(AllocateBinding());
			bindings[bindId]->BindFile(destination, priority);
		}

		return eBindError_None;
	}

	return eBindError_NotFound;
}

EBindError FileBinder::BindDirectory(const char* path, const char* destination, int priority)
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

	//if (priority == 0)
	//{
	//	auto* node = entry;
	//	while (node != nullptr)
	//	{
	//		if (node->has_data<size_t>())
	//		{
	//			const auto& bind = bindings[node->get_data<size_t>()].value();
	//			for (const auto& b : bind.binds)
	//			{
	//				if (b.priority == priority)
	//				{
	//					priority = b.priority - 1;
	//				}
	//			}
	//		}

	//		node = node->parent;
	//	}
	//}

	size_t bindId = 0;
	if (entry->has_data<size_t>())
	{
		bindId = entry->get_data<size_t>();
	}

	if (bindId != 0)
	{
		bindings[bindId]->BindDirectory(destination, priority);
	}
	else
	{
		bindId = entry->userdata.emplace<size_t>(AllocateBinding());
		bindings[bindId]->BindDirectory(destination, priority);
	}

	return eBindError_None;
}

EBindError FileBinder::BindDirectoryRecursive(const char* path, const char* destination)
{
	const auto result = BindDirectory(path, destination, 0);
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
		BindDirectory(fullPath.c_str(), file.path().string().c_str(), 0);
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
	const Binding::Bind* bind = nullptr;

	vfs.root->walk(path, [&](VirtualFileSystem::Entry* entry) -> bool
		{
			if (!entry->has_data<size_t>())
			{
				return true;
			}

			const auto& binding = bindings[entry->get_data<size_t>()];
			if (bind != nullptr && bind->priority >= binding->HighestPriority())
			{
				// continue
				return true;
			}

			if (entry->is_root())
			{
				std::string boundPath{};
				auto* b = binding->Query(path, boundPath);
				if (b != nullptr)
				{
					bind = b;
					fsPath = boundPath;
					error = eBindError_None;
				}
			}
			else
			{
				const auto rel = std::filesystem::relative(path, entry->full_path());
				std::string boundPath{};
				auto* b = binding->Query(rel.string().c_str(), boundPath);
				if (b != nullptr)
				{
					bind = b;
					fsPath = boundPath;
					error = eBindError_None;
				}
			}

			return true;
		}, VFS_ITERATE_RESOLVE_DIR_LINK);

	if (out != nullptr)
	{
		*out = fsPath;
	}

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

priority_queue<std::reference_wrapper<const FileBinder::Binding::Bind>, std::greater<const FileBinder::Binding::Bind>> FileBinder::CollectBindings(const char* path) const
{
	priority_queue<std::reference_wrapper<const Binding::Bind>, std::greater<const Binding::Bind>> queue;
	vfs.root->walk(path, [&](VirtualFileSystem::Entry* entry) -> bool
		{
			if (!entry->has_data<size_t>())
			{
				return true;
			}

			const auto& binding = bindings[entry->get_data<size_t>()];
			if (entry->is_root() || entry->parent->is_root())
			{
				std::string fsPath{};
				auto* bind = binding->Query(path, fsPath);
				if (bind != nullptr)
				{
					queue.insert(*bind);
				}
			}
			else
			{
				const auto rel = std::filesystem::relative(path, entry->full_path());
				std::string fsPath{};
				auto* bind = binding->Query(rel.string().c_str(), fsPath);
				if (bind != nullptr)
				{
					queue.insert(*bind);
				}
			}

			return true;
		}, VFS_ITERATE_RESOLVE_DIR_LINK);

	return queue;
}

FileBinder::Binding& FileBinder::GetFreeBinding()
{
	return bindings[AllocateBinding()].value();
}
