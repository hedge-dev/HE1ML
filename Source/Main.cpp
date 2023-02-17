#include <Windows.h>
#include "ModLoader.h"
#include "Game.h"
#pragma comment(linker, "/EXPORT:DirectInput8Create=C:\\Windows\\System32\\dinput8.DirectInput8Create")
ModLoader loader{};

void Init()
{
	if (Game::GetExecutingGame().id == eGameID_Unknown)
	{
		MessageBoxA(NULL, "Buy the game bitchass", "HE1ML", MB_OK);
		return;
	}

	loader.Init("cpkredir.ini");
}

BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD reason, _In_ LPVOID reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		Init();
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}

	return TRUE;
}
