#include "HostFileSystemSave.h"
#include "Globals.h"

namespace app::dev
{
	void* HostFileSystemSave::OpenFile(const char* path)
	{
		if (!g_loader->save_redirection || g_loader->save_file.empty())
		{
			return GetBaseImpl()->OpenFile(path);
		}

		if (!file_exists(g_loader->save_file.c_str()) && g_loader->save_read_through)
		{
			return GetBaseImpl()->OpenFile(path);
		}

		LOG("Loading save file: %s", g_loader->save_file.c_str());
		return GetBaseImpl()->OpenFile(g_loader->save_file.c_str());
	}

	void* HostFileSystemSave::CreateFile(const char* path)
	{
		if (!g_loader->save_redirection || g_loader->save_file.empty())
		{
			return GetBaseImpl()->CreateFile(path);
		}

		LOG("Writing save file: %s", g_loader->save_file.c_str());
		return GetBaseImpl()->CreateFile(g_loader->save_file.c_str());
	}

	bool HostFileSystemSave::EnumerateDirectory(const char* path, void* list)
	{
		return GetBaseImpl()->EnumerateDirectory(path, list);
	}

	HostFileSystemSave** ppBaseImpl = reinterpret_cast<HostFileSystemSave**>(ASLR(0x00FD4080));
	HostFileSystemSave* HostFileSystemSave::GetBaseImpl()
	{
		return *ppBaseImpl;
	}
}
