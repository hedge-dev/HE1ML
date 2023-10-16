#include "Pch.h"
#include "Mod.h"
#include <filesystem>
#include "Utilities.h"
#include "CpkAdvancedConfig.h"
#include "Game.h"
#include "Globals.h"

bool Mod::Load(const std::string& path)
{
	const std::filesystem::path modPath = path;

	this->path = path;
	root = modPath.parent_path().string();

	const auto file = read_file(path.c_str(), true);

	if (file == nullptr)
	{
		LOG("Failed to load mod: %s", path.c_str());
		return false;
	}

	const Ini ini{ reinterpret_cast<char*>(file->memory), nullptr };
	const auto mainSection = ini["Main"];
	const auto descSection = ini["Desc"];
	const auto cpksSection = ini["CPKs"];

	if (cpksSection.valid())
	{
		for (const auto property : cpksSection)
		{
			LoadAdvancedCpk(strtrim(property.value(), "\"").c_str());
		}
	}

	id = strtrim(mainSection["ID"], "\"");
	save_file = strtrim(mainSection["SaveFile"], "\"");
	title = strtrim(descSection["Title"], "\"");

	if (id.empty())
	{
		const auto hash = strhash(title);
		id.resize(8);
		snprintf(id.data(), id.size(), "%X", hash);

		id.resize(strlen(id.data()));
	}

	LOG("Loading mod: %s", title.c_str());

	const int includeDirCount = std::atoi(ini["Main"]["IncludeDirCount"]);
	char buf[32];

	for (int i = 0; i < includeDirCount; i++)
	{
		snprintf(buf, sizeof(buf), "IncludeDir%d", i);
		include_paths.push_back(strtrim(mainSection[buf], "\""));
	}

	const auto dllFilesRaw = strtrim(mainSection["DLLFile"], "\"");
	const auto dllPaths = strsplit(dllFilesRaw.c_str(), ",");

	SetDllDirectoryW(root.c_str());
	SetCurrentDirectoryW(root.c_str());

	for (const auto& dllPath : dllPaths)
	{
		LOG("    Loading DLL: %s", dllPath.c_str());
		HMODULE mod = LoadLibraryA((root / dllPath).string().c_str());
		if (!mod)
		{
			char* reason{};
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 
				GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&reason, 0, NULL);

			LOG("        Failed to load DLL: %s (%s)", dllPath.c_str(), reason);

			LocalFree(reason);
			continue;
		}

		modules.push_back(mod);
	}

	SetDllDirectoryA(nullptr);
	SetCurrentDirectoryW(loader->root_path.c_str());

	GetEvents("ProcessMessage", *reinterpret_cast<std::vector<ModEvent_t*>*>(&msg_processors));

	return true;
}

void Mod::LoadAdvancedCpk(const char* path)
{
	if (!file_exists(root / path))
	{
		return;
	}

	cpk_configs.emplace_back(path);
}

void Mod::Init(int in_bind_priority)
{
	bind_priority = (in_bind_priority * 0x10000) + include_paths.size();
	
	int i = 0;
	for (const auto& includePath : std::views::reverse(include_paths))
	{
		BindDirectory("work/", (root / includePath / "work").string().c_str(), i);

		switch (g_game->id)
		{
		case eGameID_SonicGenerations:
			BindDirectory("Sound/", (root / includePath / "Sound").string().c_str(), i);
			goto bindMovie;

		case eGameID_SonicLostWorld:
			BindDirectory("dlc/", (root / includePath / "dlc").string().c_str(), i);

			bindMovie:
			BindDirectory("movie/", (root / includePath / "movie").string().c_str(), i);
			break;

		default:
			break;
		}

		i++;
	}

	for (auto& config : cpk_configs)
	{
		const auto path = (root / config.name);
		const auto file = read_file(path.string().c_str(), true);
		if (file == nullptr)
		{
			continue;
		}

		config.Parse(reinterpret_cast<char*>(file->memory));
		config.Process(*loader->binder, path.parent_path(), bind_priority);
	}
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

void Mod::SendMessageImm(size_t id, void* data) const
{
	for (auto& processor : msg_processors)
	{
		processor(id, data);
	}
}

int Mod::BindFile(const char* source, const char* destination, int priority) const
{
	const auto rooted = path_is_rooted<char>(destination);
	if (!rooted)
	{
		const auto new_path = (root / destination).string();
		destination = new_path.c_str();

		return loader->binder->BindFile(source, destination, bind_priority + priority);
	}

	return loader->binder->BindFile(source, destination, bind_priority + priority);
}

int Mod::BindDirectory(const char* source, const char* destination, int priority) const
{
	const auto rooted = path_is_rooted<char>(destination);
	if (!rooted)
	{
		const auto new_path = (root / destination).string();
		destination = new_path.c_str();

		return loader->binder->BindDirectory(source, destination, bind_priority + priority);
	}

	return loader->binder->BindDirectory(source, destination, bind_priority + priority);
}
