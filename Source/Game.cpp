#include "Pch.h"
#include "Game.h"

Game executing_game{};

constexpr uint32_t timestamp_gens{ 0x4ED631A1 };
constexpr uint32_t timestamp_slw{ 0x5677710B };

const Game& Game::GetExecutingGame()
{
	if (executing_game.id !=  eGameID_Invalid)
	{
		return executing_game;
	}

	// Get timestamp of currently executing module using the NT header
	const auto nt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) +
		reinterpret_cast<PIMAGE_DOS_HEADER>(GetModuleHandle(nullptr))->e_lfanew);

	switch (nt_header->FileHeader.TimeDateStamp)
	{
		case timestamp_gens: 
			executing_game = { eGameID_SonicGenerations, "Sonic Generations", GetValue_Gens };
			break;

		case timestamp_slw:
			executing_game = { eGameID_SonicLostWorld, "Sonic Lost World", GetValue_SLW };
			break;

		default:
			executing_game = { eGameID_Unknown, "Unknown" };
			break;
	}

	return executing_game;
}

bool GetValue_SLW(size_t key, void* value)
{
	return false;
}

bool GetValue_Gens(size_t key, void* value)
{
	return false;
}

bool GetValue_Null(size_t key, void* value)
{
	return false;
}
