#include "LWPackfile.h"
#include <Sonic2013/Sonic2013.h>
#include <queue>

namespace lw
{
	std::queue<app::fnd::FileLoader::LoadInfo*> requests{};
	std::thread worker_thread;
	std::mutex loader_mtx{};

	struct CancellationToken
	{
		std::atomic<bool> cancelled{};
		~CancellationToken()
		{
			cancelled = true;
			if (worker_thread.joinable())
			{
				worker_thread.join();
			}
		}
	} cancellation;

	void LoaderWorker()
	{
		// NOTE: We reuse the builder to reuse its memory.
		pacx::Builder builder;

		SetThreadDescription(GetCurrentThread(), L"" __FUNCTION__);
		hh::base::InitializeWorkerThread();

		while(!cancellation.cancelled)
		{
			if (!requests.empty())
			{
				// Get the current request off of the queue.
				app::fnd::FileLoader::LoadInfo* request;

				{
					std::lock_guard guard{ loader_mtx };
					request = requests.front();
					requests.pop();
				}

				const auto handle = request->pHandle.get();

				// Collect pac files from mods
				char appendPathBuf[128];
				const auto appendPacFilePathLen = std::strlen(handle->Path);
				std::size_t pacNamePos = 0;

				std::memcpy(appendPathBuf, handle->Path, appendPacFilePathLen + 1);

				// Go backwards from the end of the append pac file path
				// until we find the position of the file name.
				for (std::size_t i = appendPacFilePathLen; i != 0; --i)
				{
					if (appendPathBuf[i] == '\\' || appendPathBuf[i] == '/')
					{
						pacNamePos = (i + 1);
						break;
					}
				}

				appendPathBuf[pacNamePos] = '+';
				std::strcpy(appendPathBuf + pacNamePos + 1, handle->Name);

				// Build a new packfile if there are any append packfiles.
				const auto bindings = g_binder->CollectBindings(appendPathBuf);

				if (!bindings.empty() &&
					// TODO: Please replace this temporary code with a better way to detect splits!!!
					appendPacFilePathLen > 3 &&
					(handle->Path.c_str())[appendPacFilePathLen - 1] == 'c' &&
					(handle->Path.c_str())[appendPacFilePathLen - 2] == 'a' &&
					(handle->Path.c_str())[appendPacFilePathLen - 3] == 'p'
					)
				{
					// Start building a new packfile.
					builder.Start(handle->pBuffer, handle->pBufferAllocator);

					// Iterate in reverse so priority is still correct when loading splits in reverse.
					for (const auto& binding : std::views::reverse(bindings))
					{
						builder.MergeWithAppend(binding.path.c_str());
					}

					// Finish building the new packfile, and modify the existing
					// request so that it refers to this new packfile instead.
					builder.Finish();
				}
				
#if 0 // Buffer override experiment
				auto old_buffer = handle->pBuffer;
				handle->pBuffer = handle->pBufferAllocator->Alloc(handle->BufferSize, 16);
				memcpy(handle->pBuffer, old_buffer, handle->Size);

				if (!(handle->BufferFlags & 1))
				{
					handle->pBufferAllocator->Free(old_buffer);
				}

				handle->BufferFlags &= ~1;
#endif

				// Execute the resource job.
				app::fnd::FileLoader::GetInstance()->ResourceJobMTExec(request);
			}

			Sleep(1);
		}
	}

	void __fastcall FileLoader_StartResourceJob(app::fnd::FileLoader* loader, void* _, app::fnd::FileLoader::LoadInfo* load_info)
	{
		std::lock_guard guard{ loader_mtx };
		LOG("Execute resource job for %s", load_info->pHandle->Path.c_str());
		requests.push(load_info);
	}
	
	HOOK(void, __fastcall, Application_InitializeMain, ASLR(0x004AB1C0), void* app)
	{
		originalApplication_InitializeMain(app);
		worker_thread = std::thread(LoaderWorker);
	}

	HOOK(int, __fastcall, GameModeStartUp_LoadResidentFile, ASLR(0x0091B700), void* game_mode)
	{
		originalGameModeStartUp_LoadResidentFile(game_mode);

		const auto allocator = csl::fnd::MallocAllocator::GetInstance();
		auto mission_appends = g_binder->CollectBindings("+actstgmission.lua");
		auto data_appends = g_binder->CollectBindings("+actstgdata.lua");
		auto* instance = app::StageInfo::CStageInfo::GetInstance();
		
		for (const auto& binding : std::views::reverse(mission_appends))
		{
			auto data = read_file(binding.path.c_str(), false);
			if (data == nullptr)
			{
				continue; // ???, bad permission?
			}

			 app::game::LuaScript lua{ allocator };
			 lua.Load(reinterpret_cast<char*>(data->memory), data->size);

			 app::StageInfo::CStageInfo stage_info{ allocator };
			 stage_info.Setup(lua);

			for(const auto& stage : stage_info.Stages)
			{
				stage->AddRef(); // Makes it deletable later

				auto* base_stage = instance->GetStageData(stage->Name.c_str());
				if (base_stage != nullptr)
				{
					stage->Zone = base_stage->Zone;
					stage->Act = base_stage->Act;

					memcpy(&base_stage->Name, &stage->Name, sizeof(app::StageInfo::SStageData) - offsetof(app::StageInfo::SStageData, Name));
				}
				else
				{
					stage->AddRef();
					bool inserted{};
					const bool in_zone = (stage->Act < 5 && stage->Zone < stage_info.Zones.size()) && stage->Name == stage_info.Zones[stage->Zone].Missions[stage->Act].c_str();
					if (in_zone)
					{
						auto& zone = instance->Zones[stage->Zone];
						zone.Missions[stage->Act] = stage->Name.c_str();
						const bool need_adjust = (zone.NumMissions < stage->Act + 1) && stage->Act < 5;

						const auto idx = zone.NumMissions;
						zone.NumMissions = std::min<>(5, stage->Act + 1);

						if (need_adjust)
						{
							const auto insert_idx = idx + zone.StageOffset;
							instance->Stages.insert(insert_idx, stage);
							inserted = true;

							// Correct other zones
							for (auto it = &zone; it != instance->Zones.end(); ++it)
							{
								if (insert_idx < it->StageOffset)
								{
									++it->StageOffset;
								}
							}
						}
					}

					if (!inserted)
					{
						instance->Stages.push_back(stage);
					}
				}
			}
		}

		instance->StageNames.clear();
		instance->SetupZoneInfo();

		for (const auto& binding : std::views::reverse(data_appends))
		{
			auto data = read_file(binding.path.c_str(), false);
			if (data == nullptr)
			{
				continue; // ???, bad permission?
			}

			app::game::LuaScript lua{ allocator };
			lua.Load(reinterpret_cast<char*>(data->memory), data->size);

			app::StageInfo::CStageInfo stage_info{ allocator };
			stage_info.ReadCategoryDebug("stage_all", lua, 0);
			stage_info.ReadCategoryDebug("old_stage_all", lua, 1);

			for (size_t i = 0; i < 2; i++)
			{
				auto& worlds = stage_info.Worlds[i];
				for (const auto& world : worlds)
				{
					for (const auto& stage : world->GetStages())
					{
						instance->AddDebugLevel(world->GetTitle(), stage.Title, stage.Name, i);
					}
				}
			}
		}

		return 0;
	}

	HOOK(void, __fastcall, Packfile_Cleanup, ASLR(0x00C19090), hh::ut::PackFile* pac)
	{
		// If this packfile has a pointer to a linked list node, cleanup all of the nodes in the list.
		if (pac->ref().Status & pacx::PACX_HEADER_STATUS_HE1ML_HAS_NEXT_NODE)
		{
			auto curNode = reinterpret_cast<pacx::LinkedListNode*>(pac->ref().Size);

			do
			{
				const auto next = curNode->next;
				curNode->allocator->Free(curNode);
				curNode = next;
			}
			while (curNode);
		}

		// Cleanup the packfile.
		originalPackfile_Cleanup(pac);
	}

	void InitWork()
	{
		WRITE_CALL(ASLR(0x0049177E), FileLoader_StartResourceJob);
		INSTALL_HOOK(Application_InitializeMain);
		INSTALL_HOOK(GameModeStartUp_LoadResidentFile);
		INSTALL_HOOK(Packfile_Cleanup);
	}
}