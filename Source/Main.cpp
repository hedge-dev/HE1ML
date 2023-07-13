#include <Windows.h>
#include "ModLoader.h"
#include "Game.h"
#include "ExceptionHandler.h"
#include <shellapi.h>

ModLoader loader{};

void Init()
{
	CommonLoader::Init();
	if (file_exists(MODLOADER_CONFIG_NAME))
	{
		loader.Init(MODLOADER_CONFIG_NAME);
		return;
	}

	loader.Init(MODLOADER_LEGACY_CONFIG_NAME);
}

HOOK(void, WINAPI, tmainCRTStartup, nullptr)
{
	int argc;
	auto* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argv)
	{
		for (int i = 1; i < argc; i++)
		{
			if (_wcsicmp(argv[i], L"-Lcri") == 0)
			{
				loader.enable_cri_logs = true;
			}
		}
		LocalFree(argv);
	}

	Init();
	originaltmainCRTStartup();
}

void Startup(HMODULE module)
{
	InitExceptionHandler();
	g_game = &Game::GetExecutingGame();

	char buffer[4096];
	GetModuleFileNameA(module, buffer, sizeof(buffer));

	ResolveStubMethods(LoadSystemLibrary(path_filename(buffer)));

	if (g_game->id == eGameID_Unknown)
	{
		char buffer[4096];
		GetModuleFileNameA(MODULE_HANDLE, buffer, sizeof(buffer));
		const char* message{ nullptr };
		if (strstr(buffer, "slw.exe"))
		{
			message = "This version of slw.exe is not supported.\nPlease use the .exe released on January 6th 2016 (12.6 MB (13,315,072 bytes)).\nMD5: CEB45AF30E1E9341032DA0C68BB3A12D";
		}
		else if (strstr(buffer, "SonicGenerations.exe"))
		{
			message = "This version of SonicGenerations.exe is not supported.\nPlease use the v1.0.0.5 .exe (23.1 MB (24,284,488 bytes)).\nMD5: FE5EA2725EE7FC51C35263D4E7C41721";
		}

		if (message != nullptr)
		{
			MessageBoxA(NULL, message, "HE1ML", MB_OK);
		}
		return;
	}

	void* crtStartup{};
	g_game->GetValue(eGameValueKey_CRTStartup, &crtStartup);
	if (crtStartup == nullptr)
	{
		MessageBoxA(NULL, "Failed to resolve CRT startup function.", "HE1ML", MB_OK);
		return;
	}

	INSTALL_HOOK_ADDRESS(tmainCRTStartup, crtStartup);
}

BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD reason, _In_ LPVOID reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		Startup(instance);
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}

	return TRUE;
}
