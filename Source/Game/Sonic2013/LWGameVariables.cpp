#include "GameVariables.h"
#include <CRIWARE/Criware.h>
#include <Game.h>

namespace lw
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

		Values()
		{
#define ASSIGN_PTR(FIELD, VALUE) *(void**)&cri_table.FIELD = (void*)VALUE

			ASSIGN_PTR(criFsIo_SelectIo, ASLR(0x009C146F));
			ASSIGN_PTR(criFsBinder_BindCpk, ASLR(0x009BAA92));
			ASSIGN_PTR(criFsiowin_Open, ASLR(0x009C3915));
			ASSIGN_PTR(criFsIoWin_Exists, ASLR(0x009C34B1));
			ASSIGN_PTR(criFsBinder_BindDirectory, ASLR(0x009B9BC0));
			ASSIGN_PTR(criFsBinder_GetStatus, ASLR(0x009BA4BA));
			ASSIGN_PTR(criFsBinder_SetPriority, ASLR(0x009B9469));

			ASSIGN_PTR(criErr_SetCallback, criErr_SetCallback);
			ASSIGN_PTR(criFsBinder_Unbind, ASLR(0x009B9D96));

			ASSIGN_PTR(criFsBinder_GetWorkSizeForBindDirectory, criFsBinder_GetWorkSizeForBindDirectory);

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

		return false;
	}
}
