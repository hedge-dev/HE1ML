// ReSharper disable CppExpressionWithoutSideEffects
#include "ModLoader.h"
#include "Game.h"
#include "Globals.h"
#include "CRIWARE/Criware.h"
#include "Utilities.h"
#include "Mod.h"

void D3D9Hooks_Init();
void ModLoader::Init(const char* configPath)
{
	g_loader = this;
	g_binder = binder.get();
	g_vfs = vfs;

	{
		root_path.resize(MAX_PATH, 0);
		do
		{
			const DWORD size = GetModuleFileNameA(GetModuleHandleA(nullptr), root_path.data(), root_path.length());
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				root_path.resize(size);
				break;
			}

		} while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);
		size_t pos = root_path.find_last_of('\\');
		if (pos == std::string::npos)
		{
			pos = root_path.find_last_of('/');
		}

		if (pos != std::string::npos)
		{
			root_path.resize(pos);
		}
		else
		{
			root_path.clear();
		}
	}

	if (!AttachConsole(ATTACH_PARENT_PROCESS))
	{
		AllocConsole();
	}

	SetCurrentDirectoryA(root_path.c_str());
	freopen("CONOUT$", "w", stdout);
	config_path = configPath;

	const auto file = std::unique_ptr<Buffer>(read_file(config_path.c_str(), true));

	const Ini ini{ reinterpret_cast<char*>(file->memory) };
	const auto cpkSection = ini["CPKREDIR"];

	save_redirection = strcmp(cpkSection["EnableSaveFileRedirection"], "0") != 0;
	save_read_through = strcmp(cpkSection["SaveFileReadThrough"], "0") != 0;
	save_file = strtrim(cpkSection["SaveFileFallback"], "\"");

	std::string dbPath = strtrim(cpkSection["ModsDbIni"], "\"");
	if (dbPath.empty())
	{
		dbPath = "Mods\\ModsDb.ini";
	}

	LoadDatabase(dbPath);
	InitCri(this);
	g_game->EventProc(eGameEvent_Init, nullptr);
	CommonLoader::RaiseInitializers();

	if (!g_game->EventProc(eGameEvent_InstallUpdateEvent, nullptr))
	{
		D3D9Hooks_Init();
	}
}

void ModLoader::LoadDatabase(const std::string& databasePath, bool append)
{
	if (!append)
	{
		mods.clear();
	}

	database_path = databasePath;
	auto codesPath = databasePath;

	path_rmfilename(codesPath.data());
	codesPath.resize(strlen(codesPath.data()));
	codesPath.append("\\Codes.dll");
	CommonLoader::LoadAssembly(codesPath.c_str());

	const auto file = std::unique_ptr<Buffer>(read_file(database_path.c_str(), true));
	if (!file)
	{
		return;
	}

	char buf[32];
	const auto ini = Ini{ reinterpret_cast<char*>(file->memory) };
	const auto mainSection = ini["Main"];
	const auto modsSection = ini["Mods"];

	const int activeModCount = std::stoi(mainSection["ActiveModCount"]);
	for (int i = activeModCount - 1; i >= 0; i--)
	{
		sprintf_s(buf, sizeof(buf), "ActiveMod%d", i);
		const auto modKey = strtrim(mainSection[buf], "\"");
		const auto* modPath = modsSection[modKey.c_str()];

		if (modPath)
		{
			RegisterMod(strtrim(modPath, "\""));
		}
	}

	v0::ModList_t list{ reinterpret_cast<const v0::Mod_t**>(mod_handles.data()), reinterpret_cast<const v0::Mod_t**>(mod_handles.data()) + mod_handles.size() };
	v0::ModInfo_t info{ &list, nullptr, 1 };

	for (size_t i = 0; i < mods.size(); i++)
	{
		info.CurrentMod = mod_handles[i].get();
		mods[i]->RaiseEvent("PostInit", &info);
	}
}

bool ModLoader::RegisterMod(const std::string& path)
{
	auto mod = std::make_unique<Mod>(this);
	
	if (!mod->Init(path))
	{
		return false;
	}

	if (!mod->save_file.empty())
	{
		save_file = (mod->root / mod->save_file).string();
	}

	auto& m = mod_handles.emplace_back(new v0::Mod_t{ mod->title.c_str(), mod->path.c_str(), mod->id.c_str(), mod.get() });
	v0::ModList_t list{ reinterpret_cast<const v0::Mod_t**>(mod_handles.data()), reinterpret_cast<const v0::Mod_t**>(mod_handles.data()) + mod_handles.size()};
	v0::ModInfo_t info{ &list, m.get(), 1 };

	mod->GetEvents("OnFrame", update_handlers);

	const auto root = mod->root.string();
	SetDllDirectoryA(root.c_str());
	SetCurrentDirectoryA(root.c_str());
	mod->RaiseEvent("Init", &info);

	SetCurrentDirectoryA(root_path.c_str());
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

void ModLoader::OnUpdate()
{
	for (const auto& handler : update_handlers)
	{
		handler(&update_info);
	}

	CommonLoader::RaiseUpdates();
}

void ModLoader::SetSaveFile(const char* path)
{
	save_file = path;

	if (save_redirection)
	{
		g_game->EventProc(eGameEvent_SetSaveFile, const_cast<char*>(path));
	}
}
