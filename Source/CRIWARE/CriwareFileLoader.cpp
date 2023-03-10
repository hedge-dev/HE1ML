#include "Pch.h"
#include "CriwareFileLoader.h"
#include "Criware.h"
#include <queue>

namespace
{
	const CriFunctionTable* cri{};
	CriFsBinderHn binder{};
	CriFsLoaderHn loader{};
	std::unordered_map<CriFsBindId, std::string> g_bind_id_map{};
	std::thread g_request_loader{};
	std::mutex g_request_mutex{};
	std::vector<std::unique_ptr<FileLoadRequest>> g_requests{};
	FileLoadRequest* g_current_request{};
}

FileLoadRequest* FindReadyRequest()
{
	for (const auto& request : g_requests)
	{
		if (request->ready && !request->finished)
		{
			return request.get();
		}
	}

	return nullptr;
}

void LoadRequest(FileLoadRequest* request)
{
	if (request == nullptr)
	{
		return;
	}
	
	request->loading = true;
	cri->criFsLoader_Load(loader, binder, request->path.c_str(), request->offset, request->load_size, request->buffer, request->buffer_size);
}

void FileLoaderWorker()
{
	while (true)
	{
		if (!g_requests.empty())
		{
			std::lock_guard guard{ g_request_mutex };
			if (g_current_request == nullptr)
			{
				g_current_request = FindReadyRequest();
				LoadRequest(g_current_request);
			}
			else
			{
				CriFsLoaderStatus status{};
				cri->criFsLoader_GetStatus(loader, &status);

				if (status != CRIFSLOADER_STATUS_LOADING)
				{
					g_current_request->finished = true;
					g_current_request->loading = false;

					g_current_request = FindReadyRequest();
					LoadRequest(g_current_request);
				}
			}
		}
		Sleep(5);
	}
}

CriError CriFileLoader_Init(const CriFunctionTable& table, size_t flags)
{
	if (CriFileLoader_IsInit())
	{
		return CRIERR_OK;
	}

	if (table.criFsBinder_Create == nullptr ||
		table.criFsLoader_Create == nullptr ||
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
	g_request_loader = std::thread(FileLoaderWorker);
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
	if ((err = cri->criFsBinder_BindCpk(binder, nullptr, path, nullptr, 0, &id)) != CRIERR_OK)
	{
		return err;
	}

	if (out_id)
	{
		*out_id = id;
	}

	g_bind_id_map[id] = path;
	CriFsBinderStatus status{};
	while (status != CRIFSBINDER_STATUS_COMPLETE)
	{
		cri->criFsBinder_GetStatus(id, &status);
		Sleep(10);
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
	while (true)
	{
		cri->criFsLoader_GetStatus(loader, &status);

		if (status != CRIFSLOADER_STATUS_LOADING)
		{
			break;
		}
		
		Sleep(1);
	}

	return err;
}

CriError CriFileLoader_LoadAsync(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size, FileLoadRequest** out_request)
{
	auto* request = new FileLoadRequest();
	request->path = path;
	request->offset = offset;
	request->load_size = load_size;
	request->buffer = buffer;
	request->buffer_size = buffer_size;
	request->finished = false;
	request->ready = true;

	std::lock_guard guard{ g_request_mutex };
	g_requests.emplace_back(request);

	if (out_request)
	{
		*out_request = request;
	}

	return CRIERR_OK;
}

CriError CriFileLoader_DeleteRequest(FileLoadRequest* request)
{
	std::lock_guard guard{ g_request_mutex };
	std::erase_if(g_requests, [&](const auto& r) { return r.get() == request; });
	return CRIERR_OK;
}

CriError CriFileLoader_IsLoadComplete(const FileLoadRequest* request, bool* out_complete)
{
	std::lock_guard guard{ g_request_mutex };
	*out_complete = request->finished;
	return CRIERR_OK;
}

bool CriFileLoader_FileExists(const char* path)
{
	CriBool exists{};
	CriFsBinderFileInfo info{};
	cri->criFsBinder_Find(binder, path, &info, &exists);
	return exists;
}

CriUint64 CriFileLoader_GetFileSize(const char* path)
{
	CriFsBinderFileInfo info{};
	cri->criFsBinder_Find(binder, path, &info, nullptr);

	return info.extract_size;
}
