#include "Pch.h"
#include "CriwareFileLoader.h"
#include "Criware.h"

namespace
{
	const CriFunctionTable* cri{};
	CriFsBinderHn binder{};
	CriFsLoaderHn loader{};
	void* work{};
	int work_size{ 0x0000230d };
}

CriError CriFileLoader_Init(const CriFunctionTable& table, size_t flags)
{
	if (CriFileLoader_IsInit())
	{
		return CRIERR_OK;
	}

	if (table.criFsBinder_Create == nullptr    || 
		table.criFsLoader_Create == nullptr    ||
		table.criFsBinder_GetStatus == nullptr ||
		table.criFsLoader_GetStatus == nullptr ||
		table.criFsBinder_Unbind == nullptr)
	{
		return CRIERR_NG;
	}

	if (!(flags & CRI_FILE_LOADER_FLAG_NO_DIR))
	{
		if (table.criFsBinder_BindDirectory == nullptr)
		{
			return CRIERR_NG;
		}
	}

	if (!(flags & CRI_FILE_LOADER_FLAG_NO_CPK))
	{
		if (table.criFsBinder_BindCpk == nullptr)
		{
			return CRIERR_NG;
		}
	}

	cri = &table;
	cri->criFsBinder_Create(&binder);
	cri->criFsLoader_Create(&loader);
	work = malloc(work_size);
	return loader != nullptr && binder != nullptr ? CRIERR_OK : CRIERR_NG;
}

bool CriFileLoader_IsInit()
{
	return cri != nullptr;
}

CriError CriFileLoader_BindCpk(const char* path)
{
	CriFsBindId id{};
	return CriFileLoader_BindCpk(path, &id);
}

CriError CriFileLoader_BindCpk(const char* path, CriFsBindId* out_id)
{
	CriFsBindId id{};
	CriError err;
	if ((err = cri->criFsBinder_BindCpk(binder, nullptr, path, work, work_size, &id)) != CRIERR_OK)
	{
		return err;
	}

	if (out_id)
	{
		*out_id = id;
	}

	CriFsBinderStatus status{};
	while (status != CRIFSBINDER_STATUS_COMPLETE)
	{
		cri->criFsBinder_GetStatus(id, &status);
		Sleep(1);
	}
	
	return err;
}

CriError CriFileLoader_BindDirectory(const char* path)
{
	CriFsBindId id;
	return CriFileLoader_BindDirectory(path, &id);
}

CriError CriFileLoader_BindDirectory(const char* path, CriFsBindId* out_id)
{
	return cri->criFsBinder_BindDirectory(binder, nullptr, path, nullptr, 0, out_id);
}

CriError CriFileLoader_Unbind(CriFsBindId id)
{
	return cri->criFsBinder_Unbind(id);
}

CriError CriFileLoader_Load(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size)
{
	CriError err = cri->criFsLoader_Load(loader, binder, path, offset, load_size, buffer, buffer_size);
	if (err != CRIERR_OK)
	{
		return err;
	}

	CriFsLoaderStatus status{};
	while(status != CRIFSLOADER_STATUS_COMPLETE)
	{
		cri->criFsLoader_GetStatus(loader, &status);
		Sleep(1);
		if (status == CRIFSLOADER_STATUS_ERROR)
		{
			break;
		}
	}

	return err;
}

bool CriFileLoader_FileExists(const char* path)
{
	CriBool exists{};
	cri->criFsBinder_Find(binder, path, nullptr, &exists);
	return exists;
}
