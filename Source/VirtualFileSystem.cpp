#include "Pch.h"
#include "VirtualFileSystem.h"
#include <filesystem>

void VirtualFileSystem::make_link(const char* path, const char* link)
{
	const std::filesystem::path p(path);
	Entry* current = make_entry(p);
	if (current->is_root())
	{
		return;
	}

	current->link = make_entry(link);
	if (!p.has_filename())
	{
		current->attribute |= eEntryAttribute_Directory;
	}
}

void VirtualFileSystem::make_deleted(const char* path)
{
	const std::filesystem::path p(path);
	Entry* entry = make_entry(p);

	if (entry->is_root())
	{
		return;
	}

	entry->attribute |= eEntryAttribute_Deleted;
	if (!p.has_filename())
	{
		entry->attribute |= eEntryAttribute_Directory;
	}

	entry->foreach([](Entry* entry) -> bool
		{
			entry->attribute |= eEntryAttribute_Deleted;
			return true;
		});
}

std::string VirtualFileSystem::Entry::full_path() const
{
	std::string path;
	const Entry* start = this;
	const Entry* current = this;
	while (current != nullptr)
	{
		if (current->is_root())
		{
			break;
		}

		path.insert(0, current->name);
		if (!current->parent->is_root())
		{
			path.insert(0, "/");
		}

		current = current->parent;
	}

	if (current != nullptr && (start->attribute & eEntryAttribute_Directory))
	{
		path.append("/");
	}

	return path.empty() ? "/" : path;
}

void VirtualFileSystem::Entry::walk(Entry* root, const char* path, const std::function<bool(Entry*)>& callback, int iterate_flags)
{
	const std::filesystem::path p(path);
	Entry* current = this;

	if (!callback(current))
	{
		return;
	}

	for (auto& part : p)
	{
		if (part.empty())
		{
			continue;
		}

		if (part == "/")
		{
			current = root;

			if (!callback(current))
			{
				return;
			}

			continue;
		}

		if (part == ".")
		{
			if (!callback(current))
			{
				return;
			}

			continue;
		}

		if (part == "..")
		{
			current = current->parent;
			if (current == nullptr)
			{
				current = root;
			}

			if (!callback(current))
			{
				return;
			}

			continue;
		}

		auto it = current->children.find(part.string());
		if (it == current->children.end())
		{
			return;
		}

		current = it->second.get();

		if (current->link != nullptr && current->link != current)
		{
			if (current->attribute & eEntryAttribute_Directory)
			{
				if (iterate_flags & VFS_ITERATE_RESOLVE_DIR_LINK)
				{
					current = current->link;
				}
			}
			else if (iterate_flags & VFS_ITERATE_RESOLVE_FILE_LINK)
			{
				current = current->link;
			}
		}

		if (!callback(current))
		{
			return;
		}
	}
}

VirtualFileSystem::Entry* VirtualFileSystem::Entry::get(Entry* root, const char* path, int iterate_flags)
{
	const std::filesystem::path p(path);
	Entry* current = this;
	for (auto& part : p)
	{
		if (part.empty())
		{
			continue;
		}

		if (part == "/")
		{
			current = root;
			continue;
		}

		if (part == ".")
		{
			continue;
		}

		if (part == "..")
		{
			current = current->parent;
			if (current == nullptr)
			{
				current = root;
			}
			continue;
		}

		auto it = current->children.find(part.string());
		if (it == current->children.end())
		{
			return nullptr;
		}

		current = it->second.get();

		if (current->is_directory() && iterate_flags & VFS_ITERATE_RESOLVE_DIR_LINK)
		{
			if (current->link != nullptr && current->link != current)
			{
				current = current->link;
			}
		}
		else if (iterate_flags & VFS_ITERATE_RESOLVE_FILE_LINK)
		{
			if (current->link != nullptr && current->link != current)
			{
				current = current->link;
			}
		}
	}

	return current;
}

VirtualFileSystem::Entry* VirtualFileSystem::Entry::make(Entry* root, const std::filesystem::path& path)
{
	Entry* current = this;
	for (auto& part : path)
	{
		if (part.empty())
		{
			continue;
		}

		if (part == ".")
		{
			continue;
		}

		if (part == "..")
		{
			current = current->parent;
			if (current == nullptr)
			{
				current = root;
			}
			continue;
		}

		auto it = current->children.find(part.string());
		if (it == current->children.end())
		{
			const auto& child = current->children[part.string()] = std::make_unique<Entry>(part.string(), current);

			// I don't know what else to do
			if (part != *(--path.end()))
			{
				child->attribute |= eEntryAttribute_Directory;
				child->attribute &= ~eEntryAttribute_Deleted;
			}

			current = child.get();
			continue;
		}
		current = it->second.get();

		if (current->is_directory() && current->link != nullptr && current->link != current)
		{
			current = current->link;
		}
	}

	if (!path.has_filename())
	{
		current->attribute |= eEntryAttribute_Directory;
	}

	return current;
}

VirtualFileSystem::Entry* VirtualFileSystem::Entry::make(Entry* root, const char* path)
{
	const std::filesystem::path p(path);
	return make(root, p);
}


VirtualFileSystem::Entry* VirtualFileSystem::get_entry(const char* path, int iterate_flags) const
{
	return root->get(path, iterate_flags);
}

VirtualFileSystem::Entry* VirtualFileSystem::make_entry(const std::filesystem::path& path) const
{
	return root->make(path);
}

VirtualFileSystem::Entry* VirtualFileSystem::make_entry(const char* path) const
{
	const std::filesystem::path p(path);
	return make_entry(p);
}
