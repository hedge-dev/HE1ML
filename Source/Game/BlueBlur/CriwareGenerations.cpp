#include "CriwareGenerations.h"
#include <Globals.h>
#include <CRIWARE/Criware.h>
#include <CRIWARE/CriwareFileLoader.h>
#include <CRIWARE/CpkRequestDevice.h>
#include <CRIWARE/CpkDevice.h>
#include <CRIWARE/ModLoaderDevice.h>

namespace
{
	std::unordered_map<CriFsBindId, CriFsBindId> g_bind_id_map{};
}


HOOK(CriError, __cdecl, criFsIo_SelectIo, nullptr, const CriChar8* path, CriFsDeviceId* device_id, CriFsIoInterfacePtr* ioif)
{
	size_t id = CriFsIoML_ResolveDevice(strformat(path).c_str());
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

CriError BindCpk(CriFsBinderHn& bndrhn, CriFsBinderHn& srcbndrhn, const CriChar8*& path, void*& work, CriSint32& worksize, CriFsBindId*& bndrid)
{
	if (!CriFileLoader_IsInit())
	{
		CriFileLoader_Init(*g_cri, {});
	}
	if (bndrid)
	{
		CriFsBindId stub_id;
		if (CriFileLoader_BindCpk(path, &stub_id) == CRIERR_OK)
		{
			g_bind_id_map[*bndrid] = stub_id;
		}
	}

	return CRIERR_OK;
}

CriError UnbindCpk(CriFsBindId& bndrid)
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

HOOK(void, __cdecl, mfCiGetFullPath, 0x00798150, const char* path, char* out)
{
	originalmfCiGetFullPath(path, out);
	const auto relativePath = std::filesystem::relative(out, g_loader->root_path);

	std::string replacePath{};
	if (g_binder->ResolvePath(relativePath.string().c_str(), &replacePath) == eBindError_None)
	{
		strcpy(out, replacePath.c_str());
	}
}

FUNCTION_PTR(void*, __cdecl, CriAuMemAlloc_Alloc, 0x007C9B39, void* allocator, size_t size, const char* name, size_t alignment);
FUNCTION_PTR(void*, __cdecl, CriAuMemAlloc_Free, 0x007C9BE8, void* allocator, void* mem);

HOOK(void*, __fastcall, CriAuObjLoc_AttachCueSheet, 0x007A5A80, size_t* ecx, void* edx, void* a1, void* a2)
{
	void* allocator = reinterpret_cast<void*>(ecx[1]);
	size_t& capacity = ecx[10];
	const size_t& count = ecx[11];
	void** data = reinterpret_cast<void**>(&ecx[12]);

	if (count >= capacity)
	{
		const size_t new_capacity = capacity * 2;
		void* new_data = CriAuMemAlloc_Alloc(allocator, new_capacity * 4, "AuObjCueSheet", 4);

		memcpy(new_data, *data, count * 4);
		CriAuMemAlloc_Free(allocator, *data);
		*data = new_data;
		capacity = new_capacity;
	}

	return originalCriAuObjLoc_AttachCueSheet(ecx, edx, a1, a2);
}

void CriGensInit()
{
	// Find the default interface
	CriFsDeviceId id{};
	CriFsIoInterfacePtr ioif{};
	g_cri->criFsIo_SelectIo(".", &id, &ioif);

	CriFsIoML_AddDevice(*ioif);
	CriFsIoML_AddDevice(cpk_io_interface);
	CriFsIoML_AddDevice(cpk_req_io_interface);

	WRITE_CALL(0x007629C5, criFsBinder_CreateOverride);
	WRITE_CALL(0x00669F13, criFs_CalculateWorkSizeForLibraryOverride);
	INSTALL_HOOK_ADDRESS(criFsIo_SelectIo, g_cri->criFsIo_SelectIo);
	INSTALL_HOOK(mfCiGetFullPath);
	INSTALL_HOOK(CriAuObjLoc_AttachCueSheet);

	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, BindCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, UnbindCpk);
}