#include "Pch.h"
#include "CriwareGenerations.h"
#include <FileBinder.h>
#include <CRIWARE/Criware.h>
#include <CRIWARE/CriwareFileLoader.h>
#include <CRIWARE/CpkRequestDevice.h>
#include "hhSharedString.h"

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
		CriFileLoader_Init(*g_cri, CRI_FILE_LOADER_FLAG_NO_DIR);
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

static size_t StringSuffix{ 0x44584946 };
Hedgehog::Base::CSharedString MakeSuffixedString(const char* str)
{
	return { str, &StringSuffix, sizeof(StringSuffix) };
}

bool HasSuffix(const Hedgehog::Base::CSharedString& str)
{
	auto* suffixPtr = ((size_t*)(str.c_str() + str.size()));
	return *suffixPtr == StringSuffix;
}

Hedgehog::Base::CSharedString ResolveFilePath(const Hedgehog::Base::CSharedString& filePath)
{
	if (HasSuffix(filePath))
	{
		return filePath;
	}

	if (strstr(filePath.c_str(), "Packed"))
	{
		LOG("PACKING");
	}

	const auto* entry = g_loader->vfs->get_entry(filePath.c_str());
	if (entry != nullptr && entry->link != nullptr)
	{
		return MakeSuffixedString(entry->full_path().c_str());
	}

	return filePath;
}

HOOK(bool, __fastcall, CFileReaderCriD3D9Open, 0x6A03B0, void* This, void* Edx, const Hedgehog::Base::CSharedString& filePath)
{
	return originalCFileReaderCriD3D9Open(This, Edx, ResolveFilePath(filePath));
}

HOOK(bool, __fastcall, CFileBinderCriExists, 0x66A140, void* This, void* Edx, const Hedgehog::Base::CSharedString& filePath)
{
	return originalCFileBinderCriExists(This, Edx, ResolveFilePath(filePath));
}

void CriGensInit(const CriFunctionTable& cri_table, ModLoader& loader)
{
	g_cri = &cri_table;
	g_binder = loader.binder.get();
	WRITE_CALL(0x007629C5, criFsBinder_CreateOverride);
	WRITE_CALL(0x00669F13, criFs_CalculateWorkSizeForLibraryOverride);

	//INSTALL_HOOK(CFileReaderCriD3D9Open);
	//INSTALL_HOOK(CFileBinderCriExists);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, BindSoundCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, UnbindSoundCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_LOAD, LoadFile);
	criFs_SetSelectIoCallback(SelectIoGens);
}