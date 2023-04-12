#include "Globals.h"
#include "HostFileSystemSave.h"

namespace lw
{
	app::dev::HostFileSystemSave saveFileSystem{};
	app::dev::HostFileSystemSave* pSaveFileSystem = &saveFileSystem;
	void InitSaveRedir()
	{
		const size_t ppSaveFs = reinterpret_cast<size_t>(&pSaveFileSystem);

		WRITE_MEMORY(ASLR(0x009010A0), size_t, ppSaveFs); // CSaveManager::CImpl::SaveDataSave
		WRITE_MEMORY(ASLR(0x0090112F), size_t, ppSaveFs); // CSaveManager::CImpl::CheckSaveFile
		WRITE_MEMORY(ASLR(0x009012B0), size_t, ppSaveFs); // CSaveManager::CImpl::SaveDataLoad
	}
}