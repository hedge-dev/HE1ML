#ifdef _WIN64
#include <SigScanner.h>
#include "GameVariables.h"
#include "CRIWARE/Criware.h"
//#include "CriwareGenerations.h"
#include "Game.h"
#include "Globals.h"

/*
HOOK(void, __fastcall, OnFrameStub, 0x006F5280, void* This)
{
	if (g_loader->update_info.device == nullptr)
	{
		g_loader->update_info.device = Sonic::CApplicationDocument::ms_pInstance->get()->m_pMember->m_spRenderingInfrastructure->m_RenderingDevice.m_pD3DDevice;
	}

	g_loader->OnUpdate();
	originalOnFrameStub(This);
}
*/

namespace gens2024
{
	CriError criFsBinder_GetWorkSizeForBindDirectory(CriFsBinderHn binder, const CriChar8* path, CriSint32* work_size)
	{
		if (work_size == nullptr)
		{
			return CRIERR_INVALID_PARAMETER;
		}

		*work_size = 88;
		return CRIERR_OK;
	}

	struct Values
	{
		CriFunctionTable cri_table;

		void InitCriTableIfNecessary()
		{
			if (cri_table.criFsIo_SelectIo) return;

			const auto moduleSize = DetourGetModuleSize(MODULE_HANDLE);

#define SIGSCAN(PATTERN, MASK)\
	CommonLoader::Scan(PATTERN, MASK, (sizeof(PATTERN) - 1), (void*)MODULE_HANDLE, moduleSize)

#define ASSIGN_PTR(FIELD, VALUE) *(void**)&cri_table.FIELD = (void*)VALUE

			ASSIGN_PTR(criFsIo_SelectIo, SIGSCAN("\x48\x89\xE0\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20\x41\x56\x48\x83\xEC\x20\x41\x83\xCE\xFF\x4C\x89\xC5\x48\x89\xD7\x44\x89\x32\x48\x8D\x15\x00\x00\x00\x00\x48\x89\xCE\x45\x8D\x46\x0A\xE8\x00\x00\x00\x00", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxx????")); // 0x14dd3d4f6
			ASSIGN_PTR(criFsBinder_BindCpk, SIGSCAN("\x48\x83\xEC\x48\x48\x8B\x44\x24\x00\xC7\x44\x24\x00\x00\x00\x00\x00\x48\x89\x44\x24\x00\x8B\x44\x24\x70\x89\x44\x24\x20\xE8\x00\x00\x00\x00\x48\x83\xC4\x48\xC3", "xxxxxxxx?xxx?????xxxx?xxxxxxxxx????xxxxx")); // 0x140804b7c
			ASSIGN_PTR(criFsiowin_Open, SIGSCAN("\x48\x89\xE0\x48\x89\x58\x10\x48\x89\x68\x18\x48\x89\x70\x20\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x50\x4D\x89\xCD\x45\x89\xC6\x89\xD6\x49\x89\xCC\x48\x85\xC9\x0F\x84\x00\x00\x00\x00\x4D\x85\xC9\x0F\x84\x00\x00\x00\x00", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxx????")); // 0x14dd45d79
			ASSIGN_PTR(criFsIoWin_Exists, SIGSCAN("\x48\x89\x5C\x24\x00\x57\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x31\xE0\x48\x89\x84\x24\x00\x00\x00\x00\x48\x89\xD3\x48\x89\xCF\x48\x85\xC9", "xxxx?xxxx????xxx????xxxxxxx????xxxxxxxxx")); // 0x14dd3f027

			// Optional Functions
			//ASSIGN_PTR(criErr_SetCallback, 0x007C92C5); // TODO
			ASSIGN_PTR(criFsBinder_Unbind, SIGSCAN("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x89\xCF\xE8\x00\x00\x00\x00\x48\x89\xC3\x48\x85\xC0\x75\x00\x48\x8D\x15\x00\x00\x00\x00\x8D\x48\x01\xE8\x00\x00\x00\x00", "xxxxxxxxxxxxx????xxxxxxx?xxx????xxxx????")); // 0x14db20068

			//ASSIGN_PTR(criFs_CalculateWorkSizeForLibrary, 0x007D0413); // TODO
			ASSIGN_PTR(criFsBinder_BindDirectory, SIGSCAN("\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x40\x48\x8B\xB4\x24\x00\x00\x00\x00\x33\xED\x49\x8B\xD9\x4D", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx?xxxxxxxxx")); // 0x140804ba4
			ASSIGN_PTR(criFsBinder_SetPriority, SIGSCAN("\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x8B\xFA\xE8\x00\x00\x00\x00\x48\x8B\xD8\x48\x85\xC0\x75\x18\x8D\x58\xFE\x33\xC9\x44\x8B\xC3\x48\x8D\x15\x00\x00\x00\x00", "xxxx?xxxxxxxx????xxxxxxxxxxxxxxxxxxx????")); // 0x140805f70
			ASSIGN_PTR(criFsBinder_GetStatus, SIGSCAN("\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x89\xD3\x89\xCF\x85\xC9\x74\x35\x48\x85\xD2\x74\x00\xE8\x00\x00\x00\x00\x48\x85\xC0\x75\x0B\xB8\x04\x00\x00\x00\x00", "xxxx?xxxxxxxxxxxxxxxxxx?x????xxxxxxx????")); // 0x14dacddb0
			ASSIGN_PTR(criFsLoader_GetStatus, SIGSCAN("\x48\x83\xEC\x28\x45\x33\xD2\x4C\x8D\x44\x24\x30\x48\x85\xD2\x4C\x8B\xC9\x4C\x0F\x45\xC2\x41\xC7\x00\x03\x00\x00\x00\x48\x85\xC9\x75\x00\x44\x8D\x41\xFE\x48\x8D\x15\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xB8\xFE\xFF\xFF\xFF", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx?xxxxxxx????x????xxxxx")); // 0x140808cc0

			//ASSIGN_PTR(crifsbinder_findWithNameEx, 0x007D335A); // TODO
			//ASSIGN_PTR(criFsBinder_Find, 0x007D38A3); // TODO
			//ASSIGN_PTR(criFsLoader_Create, 0x007D52E2); // TODO
			//ASSIGN_PTR(criFsBinder_Create, 0x007D297D); // 0x14da6e688
			//ASSIGN_PTR(criFsLoader_Load, 0x007D5026); // 0x14dbe1c6a

			ASSIGN_PTR(criFsBinder_GetWorkSizeForBindDirectory, criFsBinder_GetWorkSizeForBindDirectory);

#undef ASSIGN_PTR
		}
	} values;

	void InstallUpdateEvent()
	{
		//INSTALL_HOOK(OnFrameStub);
	}

	bool GetValue(size_t key, void** value)
	{
		if (key == eGameValueKey_CriwareTable)
		{
			values.InitCriTableIfNecessary();
			*value = &values.cri_table;
			return true;
		}
		return false;
	}

	//void InitSaveRedir();
	//void InitWork();
	bool EventProc(size_t key, void* value)
	{
		switch (key)
		{
			case eGameEvent_CriwareInit:
				//CriGensInit();
				return true;

			case eGameEvent_InstallUpdateEvent:
				InstallUpdateEvent();
				return true;

			case eGameEvent_PreInit:
				return true;

			case eGameEvent_Init:
				 //g_binder->BindDirectory("work\\", (g_loader->root_path / "work").string().c_str(), 0);
				//InitSaveRedir();
				//InitWork();
				return true;
		}

		return false;
	}
}
#endif
