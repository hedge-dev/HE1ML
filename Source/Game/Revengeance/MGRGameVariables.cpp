#include "GameVariables.h"
#include <CRIWARE/Criware.h>
#include <Game.h>

namespace mgr
{
	CriErrCbFunc& g_err_callback = *reinterpret_cast<CriErrCbFunc*>(ASLR(0x00FF8D54));
	void CRIAPI criErr_SetCallback(CriErrCbFunc cbf)
	{
		g_err_callback = cbf;
	}

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
		FUNCTION_PTR(void, WINAPI, __tmainCRTStartup, ASLR(0x00FDE9DA));

		Values()
		{
#define ASSIGN_PTR(FIELD, VALUE) *(void**)&cri_table.FIELD = (void*)VALUE

			ASSIGN_PTR(criFsIo_SelectIo, ASLR(0x0129E60F));
			ASSIGN_PTR(criFsBinder_BindCpk, ASLR(0x01298448));
			ASSIGN_PTR(criFsiowin_Open, ASLR(0x012A3D6A));
			ASSIGN_PTR(criFsIoWin_Exists, ASLR(0x012A3768));
			ASSIGN_PTR(criFsBinder_BindDirectory, ASLR(0x012972ED));
			ASSIGN_PTR(criFsBinder_GetStatus, ASLR(0x01297D56));
			ASSIGN_PTR(criFsBinder_SetPriority, ASLR(0x01296942));

			ASSIGN_PTR(criErr_SetCallback, ASLR(0x01293E36));
			ASSIGN_PTR(criFsBinder_Unbind, ASLR(0x01297543));

			ASSIGN_PTR(criFsBinder_GetWorkSizeForBindDirectory, ASLR(0x01294CE8));

			ASSIGN_PTR(criFs_CalculateWorkSizeForLibrary, ASLR(0x0129A6DF));

#undef ASSIGN_PTR
		}
	} values;

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

	CriError criFs_CalculateWorkSizeForLibraryOverride(CriFsConfig* config, CriSint32* worksize)
	{
		// We need more memory
		if (config != nullptr)
		{
			config->max_files *= 2;
			config->max_binds *= 2;
		}

		return g_cri->criFs_CalculateWorkSizeForLibrary(config, worksize);
	}

	bool EventProc(size_t key, void* value)
	{
		if (key == eGameEvent_CriwareInit)
		{
			WRITE_CALL(ASLR(0x00DE9ED2), criFs_CalculateWorkSizeForLibraryOverride);
			g_cri_config.bind_all_binders = true;
		}
		return false;
	}
}
