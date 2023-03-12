#include "CpkAdvancedConfig.h"

namespace fs = std::filesystem;
void CpkAdvancedConfig::Process(FileBinder& binder, const fs::path& root) const
{
	for (const auto& group : groups)
	{
		if (group.type == eCommandType_Add)
		{
			ProcessAdd(group, binder, root);
		}
		else if (group.type == eCommandType_Copy)
		{
			ProcessCopy(group, binder, root);
		}
	}
}

void CpkAdvancedConfig::ProcessAdd(const CommandGroup& group, FileBinder& binder, const fs::path& root)
{
	for (const auto& command : group)
	{
		const auto path = root / command.value;
		if (fs::is_directory(path))
		{
			binder.BindDirectory(command.key.c_str(), path.string().c_str());
		}
		else
		{
			binder.BindFile(command.key.c_str(), path.string().c_str());
		}
	}
}

void CpkAdvancedConfig::ProcessCopy(const CommandGroup& group, FileBinder& binder, const std::filesystem::path& root)
{
	for (const auto& command : group)
	{
		binder.vfs.make_entry(command.value);
		binder.vfs.make_link(command.key.c_str(), command.value);
	}
}


void CpkAdvancedConfig::Parse(const char* config)
{
	const Ini ini{ config };
	const auto mainSection = ini["Main"];

	const int commandCount = std::atoi(mainSection["CommandCount"]);
	char buf[32];
	for (int i = 0; i < commandCount; i++)
	{
		sprintf(buf, "Command%d", i);
		const auto commandKey = mainSection[buf];

		auto commandSplits = strsplit(commandKey, ":");
		if (commandSplits.size() != 2)
		{
			LOG("Invalid command %s", commandKey);
			continue;
		}

		const auto& command = CommandFromString(commandSplits[0].c_str());
		const auto& commandSectionKey = commandSplits[1];

		const auto commandSection = ini[commandSectionKey.c_str()];
		if (!commandSection.valid())
		{
			LOG("Invalid command section %s", commandSectionKey.c_str());
			continue;
		}

		auto& group = groups.emplace_back(command);
		group.commands.reserve(commandSection.size());

		for (const auto& prop : commandSection)
		{
			group.commands.emplace_back(prop.name(), prop.value());
		}
	}
}
