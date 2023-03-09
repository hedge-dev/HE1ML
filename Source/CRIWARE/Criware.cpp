#include "Criware.h"
#include <Shlwapi.h>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <Game.h>
#include <Game/BlueBlur/CriwareGenerations.h>

ModLoader* g_loader{};
CriFsBindId g_dir_bind{};
std::unordered_set<std::string> g_cpk_binds{};
CriFunctionTable cri{};
bool g_redir_enabled{ true };
CriFsBinderHn g_override_binder{};

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
	printf(errid, p1, p2);
	printf("\n");
}

HOOK(CriError, CRIAPI, crifsbinder_BindCpkInternal, 0x007D35F4, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid)
{
	if (!g_redir_enabled)
	{
		return originalcrifsbinder_BindCpkInternal(bndrhn, srcbndrhn, path, work, worksize, bndrid);
	}

	cri.criErr_SetCallback(criErr_Callback);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_PRE_BINDCPK, CriFsBindCpkHook_t, bndrhn, srcbndrhn, path, work, worksize, bndrid);

	std::vector<char> path_buffer{ path, path + strlen(path) + 1 };
	PathRemoveExtensionA(path_buffer.data());

	const size_t len = strlen(path_buffer.data());
	path_buffer[len] = '\\';
	path_buffer[len + 1] = '\0';
	g_cpk_binds.emplace(path_buffer.data());
	for (const auto& mod : g_loader->mods)
	{
		for (const auto& dir : mod->include_paths)
		{
			std::filesystem::path fsPath{ mod->root };
			fsPath /= dir;
			fsPath /= path_buffer.data();

			if (GetFileAttributesW(fsPath.c_str()) != INVALID_FILE_ATTRIBUTES)
			{
				g_loader->binder->BindDirectory(fsPath.string().c_str());
			}
		}
	}

	if (!g_dir_bind)
	{
		g_override_binder = bndrhn;
		cri.criFsBinder_BindDirectory(bndrhn, srcbndrhn, c_dir_stub, malloc(worksize), worksize, &g_dir_bind);
		cri.criFsBinder_SetPriority(g_dir_bind, 90000000);
	}

	printf("crifsbinder_BindCpkInternal: %s\n", path);
	const CriError err = originalcrifsbinder_BindCpkInternal(bndrhn, srcbndrhn, path, work, worksize, bndrid);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, CriFsBindCpkHook_t, bndrhn, srcbndrhn, path, work, worksize, bndrid);
	return err;
}

HOOK(CriFsIoError, CRIAPI, criFsiowin_Open, 0x007D6B1E, const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn)
{
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
			if (g_loader->binder->ResolvePath(src_path, path) == eBindError_None)
			{
				LOG("%s -> %s", src_path, path)
			}
		}
		else // others
		{
			const char* original_path = path;
			if (g_loader->binder->ResolvePath(original_path, path) == eBindError_None)
			{
				LOG("%s -> %s", original_path, path)
			}
		}
	}

	return originalcriFsiowin_Open(path, mode, access, filehn);
}

HOOK(CriFsIoError, CRIAPI, criFsIoWin_Exists, 0x007D66DB, const CriChar8* path, CriBool* exists)
{
	LOG("criFsIoWin_Exists: %s", path)
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
	//if (strstr(path, "TitleToPAM.ar.00"))
	//{
	//	void* buf = malloc(0x800000);
	//	// CriFileLoader_Load("Synth/20th_logo_Master20101228b_wav.aax", 0, 0x800000, buf, 0x800000);

	//	free(buf);
	//}

	LOG("criFsLoader_Load: %s", path);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_PRE_LOAD, CriFsLoadHook_t, loader, binder, path, offset, load_size, buffer, buffer_size);
	const CriError result =  originalcriFsLoader_Load(loader, binder, path, offset, load_size, buffer, buffer_size);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_POST_LOAD, CriFsLoadHook_t, loader, binder, path, offset, load_size, buffer, buffer_size);
	return result;
}

HOOK(CriError, CRIAPI, criFsBinder_Unbind, nullptr, CriFsBindId bndrid)
{
	LOG("criFsBinder_Unbind: %d", bndrid);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, CriFsUnbindHook_t, bndrid);
	const CriError result =  originalcriFsBinder_Unbind(bndrid);
	ML_HANDLE_CRI_HOOK(ML_CRIWARE_HOOK_POST_UNBIND, CriFsUnbindHook_t, bndrid);

	return result;
}

void InitCri(ModLoader* loader)
{
	g_loader = loader;
	//if (!Game::GetExecutingGame().GetValue(eGameValueKey_CriwareTable, &cri))
	//{
	//	LOG("Skipping CRIWARE redirection")
	//	return;
	//}

	if (Game::GetExecutingGame().id == eGameID_SonicGenerations)
	{
		CriGensInit(cri);
	}

	INSTALL_HOOK_ADDRESS(crifsbinder_BindCpkInternal, cri.criFsBinder_BindCpk);
	INSTALL_HOOK_ADDRESS(criFsIoWin_Exists, cri.criFsIoWin_Exists);
	INSTALL_HOOK_ADDRESS(criFsiowin_Open, cri.criFsiowin_Open);
	INSTALL_HOOK_ADDRESS(criFsLoader_Load, cri.criFsLoader_Load);
	INSTALL_HOOK_ADDRESS(criFsBinder_Unbind, cri.criFsBinder_Unbind);

	// Restore original addresses
	cri.criFsBinder_BindCpk = originalcrifsbinder_BindCpkInternal;
	cri.criFsIoWin_Exists = originalcriFsIoWin_Exists;
	cri.criFsiowin_Open = originalcriFsiowin_Open;
	cri.criFsLoader_Load = originalcriFsLoader_Load;
	cri.criFsBinder_Unbind = originalcriFsBinder_Unbind;
}