#include "Pch.h"
#include "VirtualFileSystem.h"
#include <filesystem>

void VirtualFileSystem::make_link(const char* path, const char* link)
{
	const std::filesystem::path p(path);
	
	Entry* current = root.get();
	for (auto& part : p)
	{
		if (part == ".")
		{
			continue;
		}

		if (part == "..")
		{
			current = current->parent;
			if (current == nullptr)
			{
				current = root.get();
			}
			continue;
		}

		auto it = current->children.find(part.string());
		if (it == current->children.end())
		{
			current->children[part.string()] = std::make_unique<Entry>(part.string(), current);
		}
		current = current->children[part.string()].get();
	}

	current->link = link;
	if (!p.has_filename())
	{
		current->attribute |= eEntryAttribute_Directory;
	}
}

const std::string& VirtualFileSystem::get_link(const char* path) const
{
	const std::filesystem::path p(path);

	Entry* current = root.get();
	for (auto& part : p)
	{
		if (part == ".")
		{
			continue;
		}

		if (part == "..")
		{
			current = current->parent;
			if (current == nullptr)
			{
				current = root.get();
			}
			continue;
		}

		auto it = current->children.find(part.string());
		if (it == current->children.end())
		{
			return current->link;
		}
		current = current->children[part.string()].get();
	}

	return current->link;
}
