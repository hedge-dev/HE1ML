#include "Criware.h"
#include <Shlwapi.h>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <Game.h>
#include <Globals.h>

CriFsBindId g_dir_bind{};
std::unordered_set<std::string> g_cpk_binds{};

void* g_cri_hooks[ML_CRIWARE_HOOK_MAX]{};

#define ML_HANDLE_CRI_HOOK1(EXPR) \
	{ \
		const CriError hook_result = EXPR; \
		if (hook_result != CRIERR_OK) \
		{ \
			if (hook_result == CRIERR_ENUM_BE_SINT32) \
				return CRIERR_OK; \
			return hook_result; \
		} \
	} \

#define ML_HANDLE_CRI_HOOK(ID, TYPE, ...) \
	if (g_cri_hooks[ID] != nullptr) \
	{ \
			ML_HANDLE_CRI_HOOK1(((TYPE)g_cri_hooks[ID])(__VA_ARGS__)); \
	}

void criErr_Callback(const CriChar8* errid, CriUint32 p1, CriUint32 p2, CriUint32* parray)
{
	g_loader->WriteLog(ML_LOG_LEVEL_ERROR, ML_LOG_CATEGORY_CRIWARE, errid, p1, p2, parray);
}

HOOK(CriError, CRIAPI, crifsbinder_BindCpkInternal, 0x007D35F4, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid)
{
	if (g_cri->criErr_SetCallback != nullptr)
	{
		g_cri->criErr_SetCallback(criErr_Callback);
	}

	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_PRE_BINDCPK, CriFsBindCpkHook_t, bndrhn, srcbndrhn, path, work, worksize, bndrid);

	std::vector<char> path_buffer{ path, path + strlen(path) + 1 };
	PathRemoveExtensionA(path_buffer.data());

	const size_t len = strlen(path_buffer.data());
	path_buffer[len] = '\0';
	g_cpk_binds.emplace(path_buffer.data());
	for (const auto& mod : g_loader->mods)
	{
		for (const auto& dir : mod->include_paths)
		{
			std::filesystem::path fsPath{ mod->root };
			fsPath /= dir;

			g_loader->binder->BindDirectory(".", (fsPath / path_buffer.data()).string().c_str());
			g_loader->binder->BindDirectory(".", (fsPath / path_filename(path_buffer.data())).string().c_str());
		}
	}

	if (!g_dir_bind)
	{
		CriSint32 dirworksize{};
		g_cri->criFsBinder_GetWorkSizeForBindDirectory(bndrhn, c_dir_stub, &dirworksize);

		g_cri->criFsBinder_BindDirectory(bndrhn, srcbndrhn, c_dir_stub, malloc(dirworksize), dirworksize, &g_dir_bind);
		g_cri->criFsBinder_SetPriority(g_dir_bind, 90000000);
	}

	LOG("crifsbinder_BindCpkInternal: %s\n", path);
	const CriError err = originalcrifsbinder_BindCpkInternal(bndrhn, srcbndrhn, path, work, worksize, bndrid);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, CriFsBindCpkHook_t, bndrhn, srcbndrhn, path, work, worksize, bndrid);
	return err;
}

HOOK(CriFsIoError, CRIAPI, criFsiowin_Open, 0x007D6B1E, const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn)
{
	std::string replacePath{};
	if (!path)
	{
		return CRIFS_IO_ERROR_NG;
	}
	else
	{
		// Very fast string compare
		if (*reinterpret_cast<const uint32_t*>(path) == *reinterpret_cast<const uint32_t*>(c_dir_stub))
		{
			const char* src_path = (path + (sizeof(c_dir_stub) - 1));
			if (g_loader->binder->ResolvePath(src_path, &replacePath) == eBindError_None)
			{
				path = replacePath.c_str();
				LOG("%s -> %s", src_path, path);
			}
		}
		else // others
		{
			const char* original_path = path;
			if (g_loader->binder->ResolvePath(original_path, &replacePath) == eBindError_None)
			{
				path = replacePath.c_str();
				LOG("%s -> %s", original_path, path);
			}
		}
	}

	return originalcriFsiowin_Open(path, mode, access, filehn);
}

HOOK(CriFsIoError, CRIAPI, criFsIoWin_Exists, 0x007D66DB, const CriChar8* path, CriBool* exists)
{
	if (!path || !exists)
	{
		return CRIFS_IO_ERROR_NG;
	}

	// Very fast string compare
	if (*reinterpret_cast<const uint32_t*>(path) == *reinterpret_cast<const uint32_t*>(c_dir_stub))
	{
		path = (path + (sizeof(c_dir_stub) - 1));
		if (g_loader->binder->FileExists(path) == eBindError_None)
		{
			*exists = true;
			return CRIFS_IO_ERROR_OK;
		}
	}
	else // others
	{
		if (g_loader->binder->FileExists(path) == eBindError_None)
		{
			*exists = true;
			return CRIFS_IO_ERROR_OK;
		}
	}

	return originalcriFsIoWin_Exists(path, exists);
}

HOOK(CriError, CRIAPI, criFsLoader_Load, nullptr, CriFsLoaderHn loader,
	CriFsBinderHn binder, const CriChar8* path, CriSint64 offset,
	CriSint64 load_size, void* buffer, CriSint64 buffer_size)
{
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_PRE_LOAD, CriFsLoadHook_t, loader, binder, path, offset, load_size, buffer, buffer_size);
	const CriError result = originalcriFsLoader_Load(loader, binder, path, offset, load_size, buffer, buffer_size);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_POST_LOAD, CriFsLoadHook_t, loader, binder, path, offset, load_size, buffer, buffer_size);
	return result;
}

HOOK(CriError, CRIAPI, criFsBinder_Unbind, nullptr, CriFsBindId bndrid)
{
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, CriFsUnbindHook_t, bndrid);
	const CriError result = originalcriFsBinder_Unbind(bndrid);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_POST_UNBIND, CriFsUnbindHook_t, bndrid);

	return result;
}

void InitCri(ModLoader* loader)
{
	if (!g_game->GetValue(eGameValueKey_CriwareTable, reinterpret_cast<void**>(&g_cri)))
	{
		LOG("Skipping CRIWARE redirection, CRIWARE table not found");
		return;
	}

	if (g_cri->criFsBinder_BindCpk == nullptr ||
		g_cri->criFsIoWin_Exists == nullptr ||
		g_cri->criFsiowin_Open == nullptr ||
		g_cri->criFsBinder_BindDirectory == nullptr ||
		g_cri->criFsBinder_GetStatus == nullptr ||
		g_cri->criFsBinder_SetPriority == nullptr ||
		g_cri->criFsBinder_GetWorkSizeForBindDirectory == nullptr)
	{
		LOG("Skipping CRIWARE redirection");
		return;
	}

	g_game->EventProc(eGameEvent_CriwareInit, g_cri);

	INSTALL_HOOK_ADDRESS(crifsbinder_BindCpkInternal, g_cri->criFsBinder_BindCpk);
	INSTALL_HOOK_ADDRESS(criFsIoWin_Exists, g_cri->criFsIoWin_Exists);
	INSTALL_HOOK_ADDRESS(criFsiowin_Open, g_cri->criFsiowin_Open);

	if (g_cri->criFsLoader_Load != nullptr)
	{
		INSTALL_HOOK_ADDRESS(criFsLoader_Load, g_cri->criFsLoader_Load);
		g_cri->criFsLoader_Load = originalcriFsLoader_Load;
	}

	if (g_cri->criFsBinder_Unbind != nullptr)
	{
		INSTALL_HOOK_ADDRESS(criFsBinder_Unbind, g_cri->criFsBinder_Unbind);
		g_cri->criFsBinder_Unbind = originalcriFsBinder_Unbind;
	}

	if (g_cri->criErr_SetCallback != nullptr)
	{
		g_cri->criErr_SetCallback(criErr_Callback);
	}

	// Restore original addresses
	g_cri->criFsBinder_BindCpk = originalcrifsbinder_BindCpkInternal;
	g_cri->criFsIoWin_Exists = originalcriFsIoWin_Exists;
	g_cri->criFsiowin_Open = originalcriFsiowin_Open;
}