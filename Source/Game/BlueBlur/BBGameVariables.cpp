#include "GameVariables.h"
#include "CRIWARE/Criware.h"
#include "CriwareGenerations.h"
#include "Game.h"
#include "Globals.h"

HOOK(void, __fastcall, OnFrameStub, 0x006F5280, void* This)
{
	g_loader->OnUpdate();
	originalOnFrameStub(This);
}

namespace bb
{
	CriError criFsBinder_GetWorkSizeForBindDirectory(CriFsBinderHn binder, const CriChar8* path, CriSint32* work_size)
	{
		if (work_size == nullptr)
		{
			return CRIERR_INVALID_PARAMETER;
		}

		*work_size = 72;
		return CRIERR_OK;
	}

	struct Values
	{
		CriFunctionTable cri_table;
		FUNCTION_PTR(void, WINAPI, __tmainCRTStartup, 0x00A6BD56);

		Values()
		{
#define ASSIGN_PTR(FIELD, VALUE) *(void**)&cri_table.FIELD = (void*)VALUE

			ASSIGN_PTR(criFsIo_SelectIo, 0x007D5ACA);
			ASSIGN_PTR(criFsBinder_BindCpk, 0x007D35F4);
			ASSIGN_PTR(criFsiowin_Open, 0x007D6B1E);
			ASSIGN_PTR(criFsIoWin_Exists, 0x007D66DB);

			// Optional Functions
			ASSIGN_PTR(criErr_SetCallback, 0x007C92C5);
			ASSIGN_PTR(criFsBinder_Unbind, 0x007D2CCD);

			ASSIGN_PTR(criFs_CalculateWorkSizeForLibrary, 0x007D0413);
			ASSIGN_PTR(criFsBinder_BindDirectory, 0x007D2AB8);
			ASSIGN_PTR(criFsBinder_SetPriority, 0x007D2512);
			ASSIGN_PTR(criFsBinder_GetStatus, 0x007D3300);
			ASSIGN_PTR(criFsLoader_GetStatus, 0x007D42F1);

			ASSIGN_PTR(crifsbinder_findWithNameEx, 0x007D335A);
			ASSIGN_PTR(criFsBinder_Find, 0x007D38A3);
			ASSIGN_PTR(criFsLoader_Create, 0x007D52E2);
			ASSIGN_PTR(criFsBinder_Create, 0x007D297D);
			ASSIGN_PTR(criFsLoader_Load, 0x007D5026);

			ASSIGN_PTR(criFsBinder_GetWorkSizeForBindDirectory, criFsBinder_GetWorkSizeForBindDirectory);

#undef ASSIGN_PTR
		}
	} values;

	void InstallUpdateEvent()
	{
		INSTALL_HOOK(OnFrameStub);
	}

	bool GetValue(size_t key, void** value)
	{
		if (key == eGameValueKey_CriwareTable)
		{
			*value = &values.cri_table;
			return true;
		}
		if (key == eGameValueKey_CRTStartup)
		{
			*value = values.__tmainCRTStartup;
			return true;
		}
		return false;
	}

	void InitSaveRedir();
	void InitWork();
	bool EventProc(size_t key, void* value)
	{
		switch (key)
		{
			case eGameEvent_CriwareInit:
				CriGensInit();
				return true;

			case eGameEvent_InstallUpdateEvent:
				InstallUpdateEvent();
				return true;

			case eGameEvent_PreInit:
				g_binder->BindDirectoryRecursive("work\\", (g_loader->root_path + "\\work\\").c_str());
				return true;

			case eGameEvent_Init:
				InitSaveRedir();
				InitWork();
				return true;
		}

		return false;
	}
}
