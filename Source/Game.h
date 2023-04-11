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

bool GetValue_Null(size_t key, void** value);

struct Game
{
	EGameID id{ eGameID_Invalid };
	const char* name{ "Invalid" };
	bool (*GetValue)(size_t key, void** value) { GetValue_Null };

	static const Game& GetExecutingGame();
};