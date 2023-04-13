// ReSharper disable CppExpressionWithoutSideEffects
#include "ModLoader.h"
#include "Game.h"
#include "Globals.h"
#include "CRIWARE/Criware.h"
#include "Utilities.h"
#include "Mod.h"

void D3D9Hooks_Init();
void StdOutLogHandler(void* obj, int level, int category, const char* message, size_t p1, size_t p2, size_t* parray)
{
	if (category == ML_LOG_CATEGORY_GENERAL)
	{
		printf("[HE1ML] ");
	}
	else if (category == ML_LOG_CATEGORY_CRIWARE)
	{
		printf("[CRIWARE] ");
	}

	printf(message, p1, p2, parray);

	if (category == ML_LOG_CATEGORY_CRIWARE)
	{
		printf("\n");
	}
}

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

	SetCurrentDirectoryA(root_path.c_str());
	config_path = configPath;

	const auto file = std::unique_ptr<Buffer>(read_file(config_path.c_str(), true));

	const Ini ini{ reinterpret_cast<char*>(file->memory) };
	const auto cpkSection = ini["CPKREDIR"];

	if (strcmp(cpkSection["Enabled"], "0") == 0)
	{
		return;
	}

	save_redirection = strcmp(cpkSection["EnableSaveFileRedirection"], "0") != 0;
	save_read_through = strcmp(cpkSection["SaveFileReadThrough"], "0") != 0;
	save_file = strtrim(cpkSection["SaveFileFallback"], "\"");

	if (stricmp(strtrim(cpkSection["LogType"], "\"").c_str(), "console") == 0)
	{
		if (!AttachConsole(ATTACH_PARENT_PROCESS))
		{
			AllocConsole();
		}
		freopen("CONOUT$", "w", stdout);

		AddLogger(this, StdOutLogHandler);
	}

	std::string dbPath = strtrim(cpkSection["ModsDbIni"], "\"");
	if (dbPath.empty())
	{
		dbPath = "Mods\\ModsDb.ini";
	}

	g_game->EventProc(eGameEvent_PreInit, nullptr);

	LoadDatabase(dbPath);
	InitCri(this);

	g_game->EventProc(eGameEvent_Init, nullptr);

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
	CommonLoader::RaiseInitializers();

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

	FilterMods();

	v0::ModList_t list{ reinterpret_cast<const v0::Mod_t**>(mod_handles.data()), reinterpret_cast<const v0::Mod_t**>(mod_handles.data()) + mod_handles.size() };
	v0::ModInfo_t info{ &list, nullptr, &g_ml_api };

	for (size_t i = 0; i < mods.size(); i++)
	{
		const auto root = mods[i]->root.string();
		SetDllDirectoryA(root.c_str());
		SetCurrentDirectoryA(root.c_str());

		info.CurrentMod = mod_handles[i].get();
		mods[i]->RaiseEvent("Init", &info);
		mods[i]->GetEvents("OnFrame", update_handlers);
		mods[i]->Init();
	}

	SetCurrentDirectoryA(root_path.c_str());

	for (size_t i = 0; i < mods.size(); i++)
	{
		info.CurrentMod = mod_handles[i].get();
		mods[i]->RaiseEvent("PostInit", &info);
	}
}

bool ModLoader::RegisterMod(const std::string& path)
{
	auto mod = std::make_unique<Mod>(this);

	if (!mod->Load(path))
	{
		return false;
	}

	if (!mod->save_file.empty())
	{
		save_file = (mod->root / mod->save_file).string();
	}

	auto& m = mod_handles.emplace_back(new v0::Mod_t{ mod->title.c_str(), mod->path.c_str(), mod->id.c_str(), mod.get() });
	v0::ModList_t list{ reinterpret_cast<const v0::Mod_t**>(mod_handles.data()), reinterpret_cast<const v0::Mod_t**>(mod_handles.data()) + mod_handles.size() };
	v0::ModInfo_t info{ &list, m.get(), &g_ml_api };

	const auto root = mod->root.string();
	SetDllDirectoryA(root.c_str());
	SetCurrentDirectoryA(root.c_str());

	SetCurrentDirectoryA(root_path.c_str());

	mods.push_back(std::move(mod));
	return true;
}

void ModLoader::BroadcastMessageImm(size_t id, void* data) const
{
	for (const auto& mod : mods)
	{
		mod->SendMessageImm(id, data);
	}
}

void ModLoader::OnUpdate()
{
	CommonLoader::RaiseUpdates();

	for (const auto& handler : update_handlers)
	{
		handler(&update_info);
	}
}

void ModLoader::ProcessMessage(size_t id, void* data)
{
	if (id == ML_MSG_ADD_LOG_HANDLER && data != nullptr)
	{
		const auto* msg = static_cast<AddLogHandlerMessage_t*>(data);
		if (msg->handler == nullptr)
		{
			return;
		}

		AddLogger(msg->obj, msg->handler);
		return;
	}
	Game::Message msg{ id, data };
	g_game->EventProc(eGameEvent_ProcessMessage, &msg);
}

void ModLoader::FilterMods()
{
	std::vector<size_t> votes{};
	std::vector<std::vector<size_t>> sub_votes{};

	votes.resize(mods.size());
	sub_votes.resize(mods.size());

	for (auto& vote : sub_votes)
	{
		vote.reserve(std::min(mods.size(), static_cast<size_t>(8)));
	}

	for (size_t i = 0; i < mods.size(); i++)
	{
		for (size_t j = 0; j < mods.size(); j++)
		{
			if (i == j)
				continue;

			FilterModArguments_t args{ mod_handles[j].get(), false };
			mods[i]->RaiseEvent("FilterMod", &args);

			if (args.handled)
			{
				votes[j]++;
				sub_votes[i].push_back(j);
			}
		}
	}
	
	for (size_t i = 0; i < votes.size(); i++)
	{
		if (votes[i] != 0)
		{
			for (const auto& other : sub_votes[i])
			{
				// Circular references is vote rigging, ignore and deny allowing both
				if (std::ranges::find(sub_votes[other], i) != sub_votes[other].end())
				{
					continue;
				}
				votes[other]--;
			}
		}
	}

	for (size_t i = 0; i < votes.size(); i++)
	{
		if (votes[i] != 0)
		{
			mods.erase(i + mods.begin());
			mod_handles.erase(i + mod_handles.begin());
		}
	}
}

void ModLoader::WriteLog(int category, int sub_category, const char* message, size_t p1, size_t p2, size_t* parray) const
{
	for (const auto& handler : log_handlers)
	{
		handler.second(handler.first, category, sub_category, message, p1, p2, parray);
	}
}

void ModLoader::SetSaveFile(const char* path)
{
	if (path == nullptr)
	{
		save_redirection = false;
		save_file = "";
		return;
	}

	save_file = path;
}

unsigned int ML_API ModLoader_GetVersion()
{
	return ML_API_VERSION;
}

const v0::Mod_t* ML_API ModLoader_FindMod(const char* id)
{
	for (const auto& mod : g_loader->mod_handles)
	{
		if (_stricmp(mod->ID, id) != 0)
		{
			return mod.get();
		}
	}
	return nullptr;
}

void ML_API ModLoader_SendMessageImm(const v0::Mod_t* mod, size_t id, void* data)
{
	if (mod)
	{
		static_cast<Mod*>(mod->pImpl)->SendMessageImm(id, data);
	}
	else
	{
		g_loader->BroadcastMessageImm(id, data);
	}
}

void ML_API ModLoader_SendMessageToLoader(size_t id, void* data)
{
	g_loader->ProcessMessage(id, data);
}

int ML_API ModLoader_BindFile(const char* path, const char* destination)
{
	return g_binder->BindFile(path, destination);
}

int ML_API ModLoader_BindDirectory(const char* path, const char* destination)
{
	return g_binder->BindDirectory(path, destination);
}

void ML_API ModLoader_Log(int level, int category, const char* message, size_t p1, size_t p2, size_t* parray)
{
	g_loader->WriteLog(level, category, message, p1, p2, parray);
}

void ML_API ModLoader_SetSaveFile(const char* path)
{
	g_loader->SetSaveFile(path);
}

CMN_LOADER_DEFINE_API_EXPORT

ModLoaderAPI_t g_ml_api
{
	ModLoader_GetVersion,
	CommonLoader_GetAPIPointer,
	ModLoader_FindMod,
	ModLoader_SendMessageImm,
	ModLoader_SendMessageToLoader,
	ModLoader_BindFile,
	ModLoader_BindDirectory,
	ModLoader_Log,
	ModLoader_SetSaveFile
};