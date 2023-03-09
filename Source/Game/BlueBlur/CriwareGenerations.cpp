#include "Pch.h"
#include "CriwareGenerations.h"
#include <CRIWARE/Criware.h>
#include <CRIWARE/CriwareFileLoader.h>
#include <CRIWARE/CpkDevice.h>
#include <CRIWARE/CpkRequestDevice.h>

namespace
{
	const CriFunctionTable* cri{};
	std::unordered_map<CriFsBindId, CriFsBindId> g_bind_id_map{};
}

void crifsloader_set_status(CriFsLoaderHn loader, CriFsLoaderStatus status)
{
	criAtomic_TestAndSet(reinterpret_cast<long*>(static_cast<char*>(loader) + 156), status);
}

CriFsSelectIoCbFunc& criFsSelectIoFunc = *reinterpret_cast<CriFsSelectIoCbFunc*>(0x01B37484);
CriFsIoInterfacePtr criFsInterfaceWin = reinterpret_cast<CriFsIoInterfacePtr>(0x01815F88);
CriError criFs_SetSelectIoCallback(CriFsSelectIoCbFunc func)
{
	criFsSelectIoFunc = func;
	return CRIERR_OK;
}

CriError SelectIoGens(const CriChar8* path, CriFsDeviceId* device_id, CriFsIoInterfacePtr* ioif)
{
	LOG("SelectIoGens: %s", path);

	if (CpkRequestFromString(path))
	{
		*device_id = CRIFS_DEVICE_01;
		*ioif = &cpk_req_io_interface;
		return CRIERR_OK;
	}

	*device_id = CRIFS_DEVICE_00;
	*ioif = criFsInterfaceWin;
	return CRIERR_OK;
}

CriError criFs_CalculateWorkSizeForLibraryOverride(CriFsConfig* config, CriSint32* worksize)
{
	// We need more memory
	if (config != nullptr)
	{
		config->max_files *= 2;
		config->max_binds *= 2;
	}

	return cri->criFs_CalculateWorkSizeForLibrary(config, worksize);
}

CriError CRIAPI criFsBinder_CreateOverride(CriFsBinderHn* bndrhn)
{
	*bndrhn = nullptr;
	return CRIERR_OK;
}

CriError BindSoundCpk(CriFsBinderHn& bndrhn, CriFsBinderHn& srcbndrhn, const CriChar8*& path, void*& work, CriSint32& worksize, CriFsBindId*& bndrid)
{
	if (!CriFileLoader_IsInit())
	{
		CriFileLoader_Init(*cri, CRI_FILE_LOADER_FLAG_NO_DIR);
	}
	
	if (!bndrhn && bndrid)
	{
		LOG("Binding sound cpk: %s", path);
		CriFsBindId stub_id;
		if (CriFileLoader_BindCpk(path, &stub_id) == CRIERR_OK)
		{
			g_bind_id_map[*bndrid] = stub_id;
		}
	}

	return CRIERR_OK;
}

CriError UnbindSoundCpk(CriFsBindId& bndrid)
{
	if (g_bind_id_map.contains(bndrid))
	{
		const CriFsBindId id = g_bind_id_map[bndrid];
		g_bind_id_map.erase(bndrid);

		CriFileLoader_Unbind(id);
		return CRIERR_OK;
	}

	return CRIERR_OK;
}

CriError CRIAPI LoadFile(CriFsLoaderHn& loader, CriFsBinderHn& binder, const CriChar8*& path, CriSint64& offset, CriSint64& load_size, void*& buffer, CriSint64& buffer_size)
{
	CriBool exists{};
	criFsInterfaceWin->Exists(path, &exists);
	if (binder == nullptr && CriFileLoader_IsInit() && CriFileLoader_FileExists(path) && !exists)
	{
		FileLoadRequest* request;
		CriFileLoader_LoadAsync(path, offset, load_size, buffer, buffer_size, &request);
		thread_local static char path_buffer[64]{};
		CpkRequestToString(path_buffer, request);

		path = path_buffer;
	}

	return CRIERR_OK;
}

void CriGensInit(const CriFunctionTable& cri_table)
{
	cri = &cri_table;
	WRITE_CALL(0x007629C5, criFsBinder_CreateOverride);
	WRITE_CALL(0x00669F13, criFs_CalculateWorkSizeForLibraryOverride);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, BindSoundCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, UnbindSoundCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_LOAD, LoadFile);
	criFs_SetSelectIoCallback(SelectIoGens);
}