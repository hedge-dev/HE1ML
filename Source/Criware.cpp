#include "Pch.h"
#include "Criware.h"
#include <Shlwapi.h>
#include <string>
#include <filesystem>
#include <unordered_set>

ModLoader* g_loader{};
CriFsBindId g_dir_bind{};
std::unordered_set<std::string> g_cpk_binds{};
CriFsBinderHn g_override_binder{};

void criErr_Callback(const CriChar8* errid, CriUint32 p1, CriUint32 p2, CriUint32* parray)
{
	printf(errid, p1, p2);
	printf("\n");
}

HOOK(CriError, CRIAPI, crifsbinder_BindCpkInternal, 0x007D35F4, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid)
{
	criErr_SetCallback(criErr_Callback);
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
		criFsBinder_BindDirectory(bndrhn, srcbndrhn, c_dir_stub, malloc(worksize), worksize, &g_dir_bind);
		criFsBinder_SetPriority(g_dir_bind, 9000000);

		CriFsBinderStatus stats = CRIFSBINDER_STATUS_ERROR;
		while (stats != CRIFSBINDER_STATUS_COMPLETE)
		{
			criFsBinder_GetStatus(g_dir_bind, &stats);
		}
	}

#if 0
	if (strstr(path, "Sound") == path)
	{
		// *bndrid = g_sound_dir_bind;
		//if (!g_sound_dir_bind)
		{
			originalcrifsbinder_BindCpkInternal(bndrhn, srcbndrhn, path, work, worksize, bndrid);
			// criFsBinder_BindDirectory(g_override_binder, srcbndrhn, c_dir_stub, malloc(worksize), worksize, &g_sound_dir_bind);

			//CriFsBinderStatus stats = CRIFSBINDER_STATUS_ERROR;
			//while (stats != CRIFSBINDER_STATUS_COMPLETE)
			//{
			//	criFsBinder_GetStatus(*bndrid, &stats);
			//}
			//criFsBinder_SetPriority(g_sound_dir_bind, 9000001);

			//*bndrid = g_sound_dir_bind;
		}

		return CRIERR_OK;
	}
#endif

	printf("crifsbinder_BindCpkInternal: %s\n", path);
	return originalcrifsbinder_BindCpkInternal(bndrhn, srcbndrhn, path, work, worksize, bndrid);
}

HOOK(HANDLE, CRIAPI, crifsiowin_CreateFile, 0x007D6B1E, const CriChar8* path, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, int dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	if (!path)
	{
		return INVALID_HANDLE_VALUE;
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

	return originalcrifsiowin_CreateFile(path, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HOOK(CriError, CRIAPI, criFsIoWin_Exists, 0x007D66DB, const CriChar8* path, CriBool* exists)
{
	// LOG("criFsIoWin_Exists: %s", path)
	if (!path || !exists)
	{
		return CRIERR_INVALID_PARAMETER;
	}

	// Very fast string compare
	if (*reinterpret_cast<const uint32_t*>(path) == *reinterpret_cast<const uint32_t*>(c_dir_stub))
	{
		path = (path + (sizeof(c_dir_stub) - 1));
		if (g_loader->binder->FileExists(path) == eBindError_None)
		{
			if (strstr(path, ".aax"))
			{
				__debugbreak();
			}
			*exists = true;
			return CRIERR_OK;
		}
	}
	else // others
	{
		if (g_loader->binder->FileExists((path + (sizeof(c_dir_stub) - 1))) == eBindError_None)
		{
			*exists = true;
			return CRIERR_OK;
		}
	}

	return originalcriFsIoWin_Exists(path, exists);
}

HOOK(CriError, CRIAPI, criFsLoader_Load, 0x007D5026, CriFsLoaderHn loader,
	CriFsBinderHn binder, const CriChar8* path, CriSint64 offset,
	CriSint64 load_size, void* buffer, CriSint64 buffer_size)
{
	//if (g_override_binder && binder)
	//{
	//	binder = g_override_binder;
	//}

	return originalcriFsLoader_Load(loader, binder, path, offset, load_size, buffer, buffer_size);
}

CriError CRIAPI criFsBinder_CreateOverride(CriFsBinderHn* bndrhn)
{
	if (g_override_binder)
	{
		*bndrhn = g_override_binder;
		return CRIERR_OK;
	}

	return CRIERR_NG;
}

void InitCri(ModLoader* loader)
{
	g_loader = loader;
	WRITE_CALL(0x007629C5, criFsBinder_CreateOverride);
	INSTALL_HOOK(crifsbinder_BindCpkInternal);
	INSTALL_HOOK(criFsIoWin_Exists);
	INSTALL_HOOK(crifsiowin_CreateFile);
	INSTALL_HOOK(criFsLoader_Load);
}