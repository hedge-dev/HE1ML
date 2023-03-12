#include "Pch.h"
#include "VirtualFileSystem.h"
#include <filesystem>

void VirtualFileSystem::make_link(const char* path, const std::string& link)
{
	const std::filesystem::path p(path);
	Entry* current = make_entry(p);
	if (current->is_root())
	{
		return;
	}

	current->link = link;
	if (!p.has_filename())
	{
		current->attribute |= eEntryAttribute_Directory;
	}
}

void VirtualFileSystem::make_link(const char* path, const char* link)
{
	make_link(path, std::string(link));
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

VirtualFileSystem::Entry* VirtualFileSystem::Entry::get(Entry* root, const char* path, bool resolve_link)
{
	const std::filesystem::path p(path);
	Entry* current = this;
	for (auto& part : p)
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
			return nullptr;
		}

		current = it->second.get();

		if (resolve_link && !current->link.empty() && current->link != current->name)
		{
			current = root->get(current->link.c_str());
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

		if (!current->link.empty() && current->link != current->name)
		{
			auto* next = root->get(root, current->link.c_str());
			if (next == nullptr)
			{
				current = root->make(root, current->link);
			}
			else
			{
				current = next;
			}
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


VirtualFileSystem::Entry* VirtualFileSystem::get_entry(const char* path, bool resolve_link) const
{
	return root->get(path, resolve_link);
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
