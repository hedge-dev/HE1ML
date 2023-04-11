#include <Windows.h>
#include "ModLoader.h"
#include "Game.h"
#include "Globals.h"

ModLoader loader{};

void Init()
{
	CommonLoader::Init();
	loader.Init("cpkredir.ini");
}

HOOK(void, WINAPI, tmainCRTStartup, nullptr)
{
	Init();
	originaltmainCRTStartup();
}

void Startup()
{
	g_game = &Game::GetExecutingGame();
	ResolveStubMethods(LoadSystemLibrary("d3d9.dll"));

	if (g_game->id == eGameID_Unknown)
	{
		char buffer[4096];
		GetModuleFileNameA(MODULE_HANDLE, buffer, sizeof(buffer));
		const char* message{ "Unsupported game detected." };
		if (strstr(buffer, "slw.exe"))
		{
			message = "Unsupported version of slw.exe found.\nSupported MD5: CEB45AF30E1E9341032DA0C68BB3A12D";
		}
		else if (strstr(buffer, "SonicGenerations.exe"))
		{
			message = "Unsupported version of SonicGenerations.exe found.\nSupported MD5: FE5EA2725EE7FC51C35263D4E7C41721";
		}

		MessageBoxA(NULL, message, "HE1ML", MB_OK);
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
		Startup();
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}

	return TRUE;
}
