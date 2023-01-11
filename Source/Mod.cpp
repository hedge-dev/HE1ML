#include "Pch.h"
#include "Mod.h"
#include <filesystem>
#include "Utilities.h"

bool Mod::Init(const std::string& path)
{
	const std::filesystem::path modPath = path;
	root = modPath.parent_path().string();

	const auto file = std::unique_ptr<Buffer>(read_file(path.c_str()));
	auto* ini = ini_load(reinterpret_cast<char*>(file->memory), nullptr);
	const int mainSection = ini_find_section(ini, "Main", 0);
	const int descSection = ini_find_section(ini, "Desc", 0);

	title = string_trim(ini_property_value(ini, descSection, ini_find_property(ini, descSection, "Title", 0)), "\"");
	LOG("Loading mod %s", title.c_str())

	const int includeDirCount = std::stoi(ini_property_value(ini, mainSection, ini_find_property(ini, mainSection, "IncludeDirCount", 0)));
	char buf[32];

	for (int i = 0; i < includeDirCount; i++)
	{
		sprintf_s(buf, "IncludeDir%d", i);
		include_paths.push_back(string_trim(ini_property_value(ini, mainSection, ini_find_property(ini, mainSection, buf, 0)), "\""));
	}

	const auto dllFilesRaw = string_trim(ini_property_value(ini, mainSection, ini_find_property(ini, mainSection, "DLLFile", 0)), "\"");
	ini_destroy(ini);

	std::vector<std::string> dllPaths{};
	string_split(dllFilesRaw.c_str(), ",", dllPaths);

	SetDllDirectoryA(root.c_str());
	for (const auto& dllPath : dllPaths)
	{
		LOG("\t\tLoading DLL %s", dllPath.c_str());
		HMODULE mod = LoadLibraryA(dllPath.c_str());
		if (!mod)
		{
			LOG("\t\t\tFailed to load DLL %s", dllPath.c_str());
			continue;
		}

		modules.push_back(mod);
	}

	SetDllDirectoryA(nullptr);
	GetEvents("ProcessMessage", msg_processors);
	return true;
}

void Mod::RaiseEvent(const char* name, void* params) const
{
	for (auto& module : modules)
	{
		auto* pEvent = static_cast<void*>(GetProcAddress(module, name));
		if (pEvent)
		{
			reinterpret_cast<ModEvent_t*>(pEvent)(params);
		}
	}
}

int Mod::GetEvents(const char* name, std::vector<ModEvent_t*>& out) const
{
	int count = 0;
	for (auto& module : modules)
	{
		auto* pEvent = reinterpret_cast<ModEvent_t*>(GetProcAddress(module, name));
		if (pEvent)
		{
			out.push_back(pEvent);
			++count;
		}
	}

	return count;
}

void Mod::SendMessageImm(void* message) const
{
	for (auto& processor : msg_processors)
	{
		processor(message);
	}
}
