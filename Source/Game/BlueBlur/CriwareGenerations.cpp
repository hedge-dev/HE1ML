#include "Pch.h"
#include "CriwareGenerations.h"
#include <FileBinder.h>
#include <CRIWARE/Criware.h>
#include <CRIWARE/CriwareFileLoader.h>
#include <CRIWARE/CpkRequestDevice.h>
#include <CRIWARE/CpkDevice.h>
#include <CRIWARE/ModLoaderDevice.h>

namespace
{
	const CriFunctionTable* g_cri{};
	FileBinder* g_binder{};
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

	if (strstr(path, "HML\\"))
	{
		path += 4;
	}

	size_t id = CriFsIoML_ResolveDevice(path);
	if (id == -1)
	{
		id = 0;
	}

	if (id > 1)
	{
		id = 1;
	}

	*ioif = &ml_io_interface;
	*device_id = static_cast<CriFsDeviceId>(id);
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

	return g_cri->criFs_CalculateWorkSizeForLibrary(config, worksize);
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
		CriFileLoader_Init(*g_cri, {});
	}
	if (/*!bndrhn && */bndrid)
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


void CriGensInit(const CriFunctionTable& cri_table, ModLoader& loader)
{
	CriFsIoML_AddDevice(*criFsInterfaceWin);
	CriFsIoML_AddDevice(cpk_io_interface);
	CriFsIoML_AddDevice(cpk_req_io_interface);
	CriFsIoML_SetVFS(loader.vfs);

	g_cri = &cri_table;
	g_binder = loader.binder.get();
	WRITE_CALL(0x007629C5, criFsBinder_CreateOverride);
	WRITE_CALL(0x00669F13, criFs_CalculateWorkSizeForLibraryOverride);
	
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, BindSoundCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, UnbindSoundCpk);
	criFs_SetSelectIoCallback(SelectIoGens);
}