#pragma once

enum EGameID
{
	eGameID_Unknown,
	eGameID_SonicLostWorld,
	eGameID_SonicGenerations,

	eGameID_Invalid = -1,
};

enum EGameValueKey
{
	eGameValueKey_CriwareTable,
	eGameValueKey_CRTStartup,
};

enum EGameEventKey
{
	eGameEvent_CriwareInit,
	eGameEvent_InstallUpdateEvent,
	eGameEvent_Init,
	eGameEvent_SetSaveFile,
};

bool GetValue_Null(size_t key, void** value);
bool EventProc_Null(size_t key, void* value);

struct Game
{
	EGameID id{ eGameID_Invalid };
	const char* name{ "Invalid" };
	bool (*GetValue)(size_t key, void** value) { GetValue_Null };
	bool (*EventProc)(size_t key, void* value) { EventProc_Null };

	static const Game& GetExecutingGame();
};