#pragma once
#include "Utilities.h"
#include <filesystem>

class VirtualFileSystem;
class FileBinder;
struct CpkAdvancedConfig
{
	enum ECommandType
	{
		eCommandType_Unknown,
		eCommandType_Add,
		eCommandType_Remove,
		eCommandType_Copy,
		eCommandType_Move,
		eCommandType_Swap,
	};

	inline static constexpr const char* CommandToString(ECommandType type)
	{
		switch (type)
		{
		case eCommandType_Add:
			return "Add";

		case eCommandType_Remove:
			return "Remove";

		case eCommandType_Copy:
			return "Copy";

		case eCommandType_Move:
			return "Move";

		case eCommandType_Swap:
			return "Swap";

		default:
			break;
		}

		return "Unknown";
	}

	inline static constexpr ECommandType CommandFromString(const char* str)
	{
		switch (strhash<true>(str))
		{
		case strhash<true>("Add"):
			return eCommandType_Add;

		case strhash<true>("Remove"):
		case strhash<true>("Delete"):
			return eCommandType_Remove;

		case strhash<true>("Copy"):
			return eCommandType_Copy;

		case strhash<true>("Move"):
			return eCommandType_Move;

		case strhash<true>("Swap"):
			return eCommandType_Swap;

		default:
			break;
		}
		return eCommandType_Unknown;
	}

	struct Command
	{
		std::string key{};
		std::string value{};
	};

	struct CommandGroup
	{
		ECommandType type{};
		std::string name{};
		std::vector<Command> commands{};

		auto begin() const
		{
			return commands.begin();
		}

		auto end() const
		{
			return commands.end();
		}
	};

	std::string name{};
	std::vector<CommandGroup> groups{};

	void Process(FileBinder& binder, const std::filesystem::path& root) const;
	static void ProcessAdd(const CommandGroup& group, FileBinder& binder, const std::filesystem::path& root);
	static void ProcessCopy(const CommandGroup& group, FileBinder& binder, const std::filesystem::path& root);
	static void ProcessMove(const CommandGroup& group, FileBinder& binder, const std::filesystem::path& root);
	static void ProcessSwap(const CommandGroup& group, FileBinder& binder, const std::filesystem::path& root);
	void Parse(const char* config);
};
