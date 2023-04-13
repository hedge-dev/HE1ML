#include "Pch.h"
#include "CriwareFileLoader.h"
#include "Criware.h"
#include <queue>

namespace
{
	struct CriBindInfo
	{
		std::string name{};
		std::unique_ptr<uint8_t[]> work{};
		size_t work_size{};
	};

	const CriFunctionTable* cri{};
	CriFsBinderHn g_main_binder{};
	std::vector<std::pair<CriFsLoaderHn, FileLoadRequest*>> g_loaders{};
	std::unordered_map<CriFsBindId, CriBindInfo> g_bind_id_map{};
	std::thread g_request_worker{};
	std::mutex g_request_mutex{};
	std::vector<std::unique_ptr<FileLoadRequest>> g_requests{};

	// Global struct to cancel the worker when app exits
	struct CancellationToken
	{
		std::atomic<bool> cancelled{};

		~CancellationToken()
		{
			cancelled = true;
			if (g_request_worker.joinable())
			{
				g_request_worker.join();
			}
		}
	} g_cancellation;
}

FileLoadRequest* FindReadyRequest()
{
	for (const auto& request : g_requests)
	{
		if (request->state == FILE_LOAD_REQUEST_STATE_READY)
		{
			return request.get();
		}
	}

	return nullptr;
}


void FileLoaderWorker()
{
	while (!g_cancellation.cancelled)
	{
		if (!g_requests.empty())
		{
			std::lock_guard guard{ g_request_mutex };
			for (auto& loaderPair : g_loaders)
			{
				if (loaderPair.second == nullptr)
				{
					loaderPair.second = FindReadyRequest();
					if (loaderPair.second != nullptr)
					{
						loaderPair.second->state = FILE_LOAD_REQUEST_STATE_LOADING;
						cri->criFsLoader_Load(loaderPair.first, g_main_binder, loaderPair.second->path.c_str(), loaderPair.second->offset, loaderPair.second->load_size, loaderPair.second->buffer, loaderPair.second->buffer_size);
					}
				}

				if (loaderPair.second != nullptr)
				{
					CriFsLoaderStatus status{};
					cri->criFsLoader_GetStatus(loaderPair.first, &status);
					loaderPair.second->state = static_cast<uint8_t>(status);

					if (status != CRIFSLOADER_STATUS_LOADING)
					{
						if (loaderPair.second->callback)
						{
							loaderPair.second->callback(*loaderPair.second);
						}
						loaderPair.second = nullptr;
					}
				}
			}
		}
		Sleep(5);
	}
}

CriError CriFileLoader_Init(const CriFunctionTable& table, const CriFileLoaderConfig& config)
{
	const auto flags = config.flags;
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
	cri->criFsBinder_Create(&g_main_binder);

	for (size_t i = 0; i < config.num_loaders; i++)
	{
		CriFsLoaderHn loader{};
		cri->criFsLoader_Create(&loader);

		if (loader != nullptr)
		{
			g_loaders.emplace_back(loader, nullptr);
		}
	}

	g_request_worker = std::thread(FileLoaderWorker);
	return !g_loaders.empty() && g_main_binder != nullptr ? CRIERR_OK : CRIERR_NG;
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
	constexpr size_t work_size = 0x400000;
	auto work = std::make_unique<uint8_t[]>(work_size);

	if ((err = cri->criFsBinder_BindCpk(g_main_binder, nullptr, path, work.get(), work_size, &id)) != CRIERR_OK)
	{
		return err;
	}

	if (out_id)
	{
		*out_id = id;
	}

	g_bind_id_map[id] = { path, std::move(work), work_size };

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
	return cri->criFsBinder_BindDirectory(g_main_binder, nullptr, path, nullptr, 0, out_id);
}

CriError CriFileLoader_Unbind(CriFsBindId id)
{
	const CriError result = cri->criFsBinder_Unbind(id);
	g_bind_id_map.erase(id);
	return result;
}

CriError CriFileLoader_Load(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size)
{
	FileLoadRequest* request{};
	const auto err = CriFileLoader_LoadAsync(path, offset, load_size, buffer, buffer_size, &request);
	if (err != CRIERR_OK)
	{
		return err;
	}

	bool complete{};
	while (!complete)
	{
		CriFileLoader_IsLoadComplete(request, &complete);

		// Hopefully save a millisecond
		if (complete)
		{
			break;
		}

		Sleep(1);
	}

	return err;
}

CriError CriFileLoader_LoadAsync(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size, FileLoadRequest** out_request)
{
	return CriFileLoader_LoadAsync(path, offset, load_size, buffer, buffer_size, nullptr, out_request);
}

CriError CriFileLoader_LoadAsync(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size,
	const std::function<void(const FileLoadRequest&)>& callback, FileLoadRequest** out_request)
{
	auto* request = new FileLoadRequest();
	request->path = path;
	request->offset = offset;
	request->load_size = load_size;
	request->buffer = buffer;
	request->buffer_size = buffer_size;
	request->callback = callback;
	request->state = FILE_LOAD_REQUEST_STATE_READY;

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
	std::erase_if(g_requests, [request](const auto& r) { return r.get() == request; });
	return CRIERR_OK;
}

CriError CriFileLoader_IsLoadComplete(const FileLoadRequest* request, bool* out_complete)
{
	if (request == nullptr)
	{
		*out_complete = false;
		return CRIERR_OK;
	}

	std::lock_guard guard{ g_request_mutex };
	*out_complete = request->state != FILE_LOAD_REQUEST_STATE_LOADING && request->state != FILE_LOAD_REQUEST_STATE_READY;
	return CRIERR_OK;
}

bool CriFileLoader_FileExists(const char* path)
{
	CriBool exists{};
	CriFsBinderFileInfo info{};
	cri->criFsBinder_Find(g_main_binder, path, &info, &exists);
	return exists;
}

CriUint64 CriFileLoader_GetFileSize(const char* path)
{
	CriFsBinderFileInfo info{};
	cri->criFsBinder_Find(g_main_binder, path, &info, nullptr);

	return info.extract_size;
}

CriError CriFileLoader_GetFileInfo(const char* path, CriFsBinderFileInfoTag& info)
{
	CriBool exists{};
	cri->criFsBinder_Find(g_main_binder, path, &info, &exists);
	return exists ? CRIERR_OK : CRIERR_NG;
}
