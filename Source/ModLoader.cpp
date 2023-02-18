#include "Pch.h"
#include "ModLoader.h"
#include "CRIWARE/Criware.h"
#include "Utilities.h"
#include "VirtualFileSystem.h"
#include "Mod.h"

void ModLoader::Init(const char* configPath)
{
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CONOUT$", "w", stdout);
	config_path = configPath;

	const auto file = std::unique_ptr<Buffer>(read_file(config_path.c_str()));

	auto* ini = ini_load(reinterpret_cast<char*>(file->memory), nullptr);
	const int cpkSection = ini_find_section(ini, "CPKREDIR", 0);

	std::string dbPath = string_trim(ini_property_value(ini, cpkSection, ini_find_property(ini, cpkSection, "ModsDbIni", 0)), "\"");
	if (dbPath.empty())
	{
		dbPath = "Mods\\ModsDb.ini";
	}

	LoadDatabase(dbPath);

	ini_destroy(ini);
	// SetStdHandle(STD_OUTPUT_HANDLE, CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
	InitCri(this);
}

void ModLoader::LoadDatabase(const std::string& databasePath, bool append)
{
	if (!append)
	{
		mods.clear();
	}

	database_path = databasePath;
	const auto file = std::unique_ptr<Buffer>(read_file(database_path.c_str()));
	if (!file)
	{
		return;
	}

	char buf[32];
	auto* ini = ini_load(reinterpret_cast<char*>(file->memory), nullptr);
	const int mainSection = ini_find_section(ini, "Main", 0);
	const int modsSection = ini_find_section(ini, "Mods", 0);

	const int activeModCount = std::stoi(ini_property_value(ini, mainSection, ini_find_property(ini, mainSection, "ActiveModCount", 0)));
	for (int i = 0; i < activeModCount; i++)
	{
		sprintf_s(buf, sizeof(buf), "ActiveMod%d", i);
		const auto modKey = string_trim(ini_property_value(ini, mainSection, ini_find_property(ini, mainSection, buf, 0)), "\"");
		const auto* modPath = ini_property_value(ini, modsSection, ini_find_property(ini, modsSection, modKey.c_str(), 0));

		if (modPath)
		{
			RegisterMod(string_trim(modPath, "\""));
		}
	}

	ini_destroy(ini);
}

namespace v0
{
	struct Mod
	{
		const char* Name;
		const char* Path;
	};

	struct ModInfo
	{
		std::vector<Mod*>* ModList;
		Mod* CurrentMod;
	};
}

wchar_t CurrentDirectory[2048];
bool ModLoader::RegisterMod(const std::string& path)
{
	v0::Mod m{};
	v0::ModInfo info{ nullptr, &m };
	auto mod = std::make_unique<Mod>(this);

	m.Name = mod->title.c_str();
	m.Path = path.c_str();
	if (!mod->Init(path))
	{
		return false;
	}

	GetCurrentDirectoryW(2048, CurrentDirectory);
	SetCurrentDirectoryA(mod->root.c_str());
	mod->RaiseEvent("Init", &info);
	SetCurrentDirectoryW(CurrentDirectory);
	mods.push_back(std::move(mod));
	return true;
}

void ModLoader::BroadcastMessageImm(void* message) const
{
	for (const auto& mod : mods)
	{
		mod->SendMessageImm(message);
	}
}
