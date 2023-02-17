#include "Pch.h"
#include "CriwareGenerations.h"
#include "Criware.h"
#include "CriwareFileLoader.h"

namespace
{
	const CriFunctionTable* cri{};
	std::unordered_map<CriFsBindId, CriFsBindId> g_bind_id_map{};
}

CriError criFs_CalculateWorkSizeForLibraryOverride(CriFsConfig* config, CriSint32* worksize)
{
	// We need more memory
	if (config != nullptr)
	{
		config->max_files *= 2;
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

void CriGensInit(const CriFunctionTable& cri_table)
{
	cri = &cri_table;
	WRITE_CALL(0x007629C5, criFsBinder_CreateOverride);
	WRITE_CALL(0x00669F13, criFs_CalculateWorkSizeForLibraryOverride);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_POST_BINDCPK, BindSoundCpk);
	ML_SET_CRIWARE_HOOK(ML_CRIWARE_HOOK_PRE_UNBIND, UnbindSoundCpk);
}