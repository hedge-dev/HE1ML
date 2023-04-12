#include "Globals.h"

HOOK(bool, __fastcall, CSaveLoadTestPC_SaveContentsExist, 0x00E7A3D0, void* ecx)
{
	if (!g_loader->save_redirection)
	{
		return originalCSaveLoadTestPC_SaveContentsExist(ecx);
	}

	if (file_exists(g_loader->save_file.c_str()))
	{
		return true;
	}

	if (g_loader->save_read_through)
	{
		return originalCSaveLoadTestPC_SaveContentsExist(ecx);
	}

	return false;
}

HOOK(bool, __fastcall, CSaveLoadTestPC_SaveContentsSave, 0x00E7A320, void* ecx, void* edx, const void* buffer, size_t bufsize)
{
	if (!g_loader->save_redirection || g_loader->save_file.empty())
	{
		return originalCSaveLoadTestPC_SaveContentsSave(ecx, edx, buffer, bufsize);
	}

	const HANDLE hFile = CreateFileA(g_loader->save_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	WriteFile(hFile, buffer, bufsize, nullptr, nullptr);
	CloseHandle(hFile);

	return true;
}

HOOK(bool, __fastcall, CSaveLoadTestPC_SaveContentsRead, 0x00E7A220, void* ecx, void* edx, void* buffer, size_t bufsize)
{
	if (!g_loader->save_redirection || g_loader->save_file.empty())
	{
		return originalCSaveLoadTestPC_SaveContentsRead(ecx, edx, buffer, bufsize);
	}

	const HANDLE hFile = CreateFileA(g_loader->save_file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		if (g_loader->save_read_through)
		{
			return originalCSaveLoadTestPC_SaveContentsRead(ecx, edx, buffer, bufsize);
		}
		return false;
	}

	LOG("Loading save file from %s", g_loader->save_file.c_str());
	ReadFile(hFile, buffer, bufsize, nullptr, nullptr);
	CloseHandle(hFile);

	return true;
}

HOOK(bool, __fastcall, CSaveLoadTestPC_SaveContentsDelete, 0x00E7A1F0, void* ecx)
{
	bool deleted{};
	if (g_loader->save_redirection && !g_loader->save_file.empty())
	{
		deleted = DeleteFileA(g_loader->save_file.c_str()) != 0;
	}

	return deleted || originalCSaveLoadTestPC_SaveContentsDelete(ecx);
}

namespace bb
{
	void InitSaveRedir()
	{
		INSTALL_HOOK(CSaveLoadTestPC_SaveContentsExist);
		INSTALL_HOOK(CSaveLoadTestPC_SaveContentsSave);
		INSTALL_HOOK(CSaveLoadTestPC_SaveContentsRead);
		INSTALL_HOOK(CSaveLoadTestPC_SaveContentsDelete);
	}
}