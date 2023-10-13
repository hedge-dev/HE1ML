#include <Sonic2013/Sonic2013.h>
#include <queue>

namespace lw
{
	std::queue<app::fnd::FileLoader::LoadInfo*> requests{};
	std::mutex loader_mtx{};
	void LoaderWorker()
	{
		SetThreadDescription(GetCurrentThread(), L"" __FUNCTION__);
		hh::base::InitializeWorkerThread();

		while(true)
		{
			if (!requests.empty())
			{
				std::lock_guard guard{ loader_mtx };
				const auto& request = requests.front();

				auto handle = request->m_pHandle;
				
#if 0 // Buffer override experiment
				auto old_buffer = handle->m_pBuffer;
				handle->m_pBuffer = handle->m_pBufferAllocator->Alloc(handle->m_BufferSize, 16);
				memcpy(handle->m_pBuffer, old_buffer, handle->m_Size);

				if (!(handle->m_BufferFlags & 1))
				{
					handle->m_pBufferAllocator->Free(old_buffer);
				}

				handle->m_BufferFlags &= ~1;
#endif
				app::fnd::FileLoader::GetInstance()->ResourceJobMTExec(request);
				requests.pop();
			}

			Sleep(1);
		}
	}

	std::thread worker_thread;
	void __fastcall FileLoader_StartResourceJob(app::fnd::FileLoader* loader, void* _, app::fnd::FileLoader::LoadInfo* load_info)
	{
		std::lock_guard guard{ loader_mtx };
		LOG("Execute resource job for %s", load_info->m_pHandle->m_Path.c_str());
		requests.push(load_info);
	}
	
	HOOK(void, __fastcall, Application_InitializeMain, ASLR(0x004AB1C0), void* app)
	{
		originalApplication_InitializeMain(app);
		worker_thread = std::thread(LoaderWorker);
	}

	void InitWork()
	{
		WRITE_CALL(ASLR(0x0049177E), FileLoader_StartResourceJob);
		INSTALL_HOOK(Application_InitializeMain);
	}
}