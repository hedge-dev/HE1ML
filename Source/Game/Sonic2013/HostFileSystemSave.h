#pragma once
#undef CreateFile

namespace app::dev
{
	// Implement app::dev::HostFileSystem
	class HostFileSystemSave
	{
	public:
		virtual ~HostFileSystemSave() = default;
		virtual void* OpenFile(const char* path);
		virtual void* CreateFile(const char* path);
		virtual bool EnumerateDirectory(const char* path, void* list);

		static HostFileSystemSave* GetBaseImpl();
	};
}