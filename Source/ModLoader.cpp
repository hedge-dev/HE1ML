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
		if (!g_loader->enable_cri_logs)
		{
			return;
		}
		printf("[CRIWARE] ");
	}

	printf(message, p1, p2, parray);

	if (category == ML_LOG_CATEGORY_CRIWARE)
	{
		printf("\n");
	}
}

void ModLoader::BasicInit()
{
	g_loader = this;
	g_binder = binder.get();
	g_vfs = vfs;

	{
		std::wstring pathBuf(MAX_PATH, 0);
		do
		{
			const DWORD size = GetModuleFileNameW(GetModuleHandleW(nullptr), pathBuf.data(), pathBuf.length());
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				pathBuf.resize(pathBuf.length() * 2);
			}
			else
			{
				pathBuf.resize(size);
				break;
			}

		} while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);

		root_path = pathBuf;
		root_path.remove_filename();
	}
}

void ModLoader::Init(const char* configPath)
{
	SetCurrentDirectoryW(root_path.c_str());
	config_path = configPath;

	const bool isLegacy = stricmp(path_filename(configPath), MODLOADER_LEGACY_CONFIG_NAME) == 0;

	const auto file = read_file(config_path.c_str(), true);

	const Ini ini{ reinterpret_cast<char*>(file != nullptr ? file->memory : nullptr) };
	const auto cpkSection = ini[isLegacy ? "CPKREDIR" : "HEDGEHOG"];

	if (strcmp(cpkSection["Enabled"], "0") == 0)
	{
		return;
	}

	sys_mod.id = "modloader.system";
	sys_mod.title = "ModLoader.System";
	sys_mod.path = root_path.string();
	save_redirection = strcmp(cpkSection["EnableSaveFileRedirection"], "0") != 0;
	save_read_through = strcmp(cpkSection["SaveFileReadThrough"], "0") != 0;
	save_file = strtrim(cpkSection["SaveFileFallback"], "\"");

#if !defined(DEBUG)
	if (stricmp(strtrim(cpkSection["LogType"], "\"").c_str(), "console") == 0)
#endif
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

	const auto file = read_file(database_path.c_str(), true);
	if (!file)
	{
		return;
	}

	char buf[32];
	const auto ini = Ini{ reinterpret_cast<char*>(file->memory) };
	const auto mainSection = ini["Main"];
	const auto modsSection = ini["Mods"];

	const int activeModCount = std::stoi(mainSection["ActiveModCount"]);
	for (int i = 0; i < activeModCount; i++)
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
	v0::ModInfo_t info{ &list, &sys_mod_handle, &g_ml_api };

	for (size_t i = mods.size() - 1; i != -1; i--)
	{
		SetDllDirectoryW(mods[i]->root.c_str());
		SetCurrentDirectoryW(mods[i]->root.c_str());

		info.CurrentMod = mod_handles[i].get();
		mods[i]->RaiseEvent("PreInit", &info);
		mods[i]->GetEvents("OnFrame", update_handlers);
	}

	info.CurrentMod = &sys_mod_handle;
	sys_mod.RaiseEvent("PreInit", &info);
	sys_mod.GetEvents("OnFrame", update_handlers);

	std::ranges::sort(mod_handles, [](const auto& a, const auto& b) { return a->Priority < b->Priority; });
	for (size_t i = 0; i < mod_handles.size(); i++)
	{
		mods[i].release();  // NOLINT(bugprone-unused-return-value)
		mods[i].reset(static_cast<Mod*>(mod_handles[i]->pImpl));
	}
	
	for (size_t i = mods.size() - 1; i != -1; i--)
	{
		SetDllDirectoryW(mods[i]->root.c_str());
		SetCurrentDirectoryW(mods[i]->root.c_str());

		info.CurrentMod = mod_handles[i].get();
		mods[i]->RaiseEvent("Init", &info);
		mods[i]->Init(mods.size() - i);
	}

	info.CurrentMod = &sys_mod_handle;
	sys_mod.RaiseEvent("Init", &info);
	CommonLoader::RaiseInitializers();

	for (size_t i = mods.size() - 1; i != -1; i--)
	{
		SetDllDirectoryW(mods[i]->root.c_str());
		SetCurrentDirectoryW(mods[i]->root.c_str());
		info.CurrentMod = mod_handles[i].get();
		mods[i]->RaiseEvent("PostInit", &info);
	}

	info.CurrentMod = &sys_mod_handle;
	sys_mod.RaiseEvent("PostInit", &info);
	SetCurrentDirectoryW(root_path.c_str());
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

	mod_handles.emplace_back(new v0::Mod_t{ mod->title.c_str(), mod->path.c_str(), mod->id.c_str(), mods.size(), mod.get() });

	mods.push_back(std::move(mod));
	return true;
}

void ModLoader::BroadcastMessageImm(size_t id, void* data) const
{
	for (const auto& mod : mods)
	{
		mod->SendMessageImm(id, data);
	}

	sys_mod.SendMessageImm(id, data);
}

bool ModLoader::LoadExternalModule(const char* path, bool raise_init)
{
	const auto library = LoadLibraryA(path);
	if (library == nullptr)
	{
		return false;
	}

	const auto idx = sys_mod.modules.size();
	sys_mod.modules.push_back(library);

	if (raise_init)
	{
		v0::ModList_t list{ reinterpret_cast<const v0::Mod_t**>(mod_handles.data()), reinterpret_cast<const v0::Mod_t**>(mod_handles.data()) + mod_handles.size() };
		v0::ModInfo_t info{ &list, &sys_mod_handle, &g_ml_api };

		const std::filesystem::path module_path = path;
		SetCurrentDirectoryW(module_path.parent_path().c_str());
		sys_mod.RaiseEvent(idx, "Init", &info);

		SetCurrentDirectoryW(root_path.c_str());
	}

	return true;
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

			FilterModArguments_t args{ mod_handles[j].get(), mod_handles[i].get(), false };
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
		if (_stricmp(mod->ID, id) == 0)
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

int ML_API ModLoader_BindFile(const Mod_t* mod, const char* path, const char* destination, int priority)
{
	const auto* impl = static_cast<Mod*>(mod->pImpl);
	return impl->BindFile(path, destination, priority);
}

int ML_API ModLoader_BindDirectory(const Mod_t* mod, const char* path, const char* destination, int priority)
{
	const auto* impl = static_cast<Mod*>(mod->pImpl);
	return impl->BindFile(path, destination, priority);
}

int ML_API ModLoader_BindFileEx(const char* path, const char* destination, int priority)
{
	return g_binder->BindFile(path, destination, priority);
}

int ML_API ModLoader_BindDirectoryEx(const char* path, const char* destination, int priority)
{
	return g_binder->BindDirectory(path, destination, priority);
}

const Mod_t* ML_API ModLoader_FindModEx(const void* data, int type)
{
	std::function<bool(Mod*, const void*)> selector{};
	if (type >= ML_PROPERTY_TYPE_COUNT)
	{
		return nullptr;
	}

	if (type == ML_PROPERTY_TYPE_ID)
	{
		selector = [](Mod* mod, const void* data) { return _stricmp(mod->id.c_str(), static_cast<const char*>(data)) == 0; };
	}
	else if (type == ML_PROPERTY_TYPE_TITLE)
	{
		selector = [](Mod* mod, const void* data) { return _stricmp(mod->title.c_str(), static_cast<const char*>(data)) == 0; };
	}
	else if (type == ML_PROPERTY_TYPE_HMODULE)
	{
		selector = [](Mod* mod, const void* data) { return std::ranges::find(mod->modules, (HMODULE)data) != mod->modules.end(); };
	}
	else if (type == ML_PROPERTY_TYPE_INDEX)
	{
		return g_loader->mod_handles[reinterpret_cast<size_t>(data)].get();
	}
	else if (type == ML_PROPERTY_TYPE_CALLER)
	{
		HMODULE module{};
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<LPCSTR>(_ReturnAddress()), &module))
		{
			return nullptr;
		}

		return ModLoader_FindModEx(module, ML_PROPERTY_TYPE_HMODULE);
	}

	if (selector != nullptr)
	{
		for (const auto& mod : g_loader->mod_handles)
		{
			if (selector(static_cast<Mod*>(mod->pImpl), data))
			{
				return mod.get();
			}
		}
	}

	return nullptr;
}

void ML_API ModLoader_Log(int level, int category, const char* message, size_t p1, size_t p2, size_t* parray)
{
	g_loader->WriteLog(level, category, message, p1, p2, parray);
}

void ML_API ModLoader_SetSaveFile(const char* path)
{
	g_loader->SetSaveFile(path);
}

bool ML_API ModLoader_LoadExternalModule(const char* path)
{
	return g_loader->LoadExternalModule(path, true);
}

size_t ML_API ModLoader_SetPriority(const Mod_t* mod, size_t priority)
{
	if (mod == nullptr)
	{
		return 0;
	}

	const size_t curIdx = std::ranges::find_if(g_loader->mod_handles, [mod](const auto& m) { return m.get() == mod; }) - g_loader->mod_handles.begin();
	if (curIdx == g_loader->mod_handles.size())
	{
		return 0;
	}

	if (priority >= g_loader->mod_handles.size())
	{
		priority = g_loader->mod_handles.size();
	}

	if (curIdx == priority)
	{
		return curIdx;
	}

	g_loader->mod_handles[curIdx]->Priority = priority;
	g_loader->mod_handles[priority]->Priority = priority + 1;

	return priority;
}

CMN_LOADER_DEFINE_API_EXPORT

extern "C" __declspec(dllexport) const ModLoaderAPI_t* ML_API ML_API_EXPORT_NAME()
{
	return &g_ml_api;
}

ModLoaderAPI_t g_ml_api
{
	ModLoader_GetVersion,
	CommonLoader_GetAPIPointer,
	ModLoader_FindMod,
	ModLoader_SendMessageImm,
	ModLoader_SendMessageToLoader,
	ModLoader_SetPriority,
	ModLoader_BindFile,
	ModLoader_BindDirectory,
	ModLoader_BindFileEx,
	ModLoader_BindDirectoryEx,
	ModLoader_FindModEx,
	ModLoader_Log,
	ModLoader_SetSaveFile,
	ModLoader_LoadExternalModule
};