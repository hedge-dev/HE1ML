#include "Pch.h"
#include "FileBinder.h"
#include "xxhash.h"

EBindError FileBinder::UnbindFile(const char* path)
{
	std::lock_guard lock(mtx);
	if (path == nullptr)
	{
		return eBindError_BadArguments;
	}

	lookup_cache.clear();
	const auto* entry = vfs.get_entry(path, false);
	if (entry && entry->is_root())
	{
		entry->parent->erase(entry);
		return eBindError_None;
	}

	return eBindError_NotFound;
}

EBindError FileBinder::BindFile(const char* path, const char* destination)
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
		vfs.make_entry(path)->userdata = reinterpret_cast<size_t>(strings.emplace(destination).first->c_str());
		return eBindError_None;
	}
	else
	{
		return eBindError_NotFound;
	}
}

EBindError FileBinder::BindDirectory(const char* path)
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

	lookup_cache.clear();

	bound_directories.push_back(pathStr);
	return eBindError_None;
}

EBindError FileBinder::FileExists(const char* path) const
{
	const char* unused{};
	return ResolvePath(path, unused);
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

	const auto* file_bind = vfs.get_entry(path);
	if (file_bind != nullptr)
	{
		out = lookup_cache.insert_or_assign(hash, reinterpret_cast<const char*>(file_bind->userdata)).first->second->c_str();
		return eBindError_None;
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