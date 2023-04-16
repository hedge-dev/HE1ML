#include "Globals.h"

#define BB_EXCLUDE_NAMESPACE_ALIASES
#include <BlueBlur.h>

HOOK(void, __fastcall, CDirectoryD3D9GetFiles, 0x667160, 
	void* This,
	void* Edx, 
	hh::list<Hedgehog::Base::CSharedString>& list,
	Hedgehog::Base::CSharedString& path,
	void* a3, 
	void* a4)
{
	g_binder->EnumerateFiles(path.c_str(), [&list](auto& x) -> bool
	{
		list.emplace_back(x.filename().string().c_str());
		return true;
	});
}

HOOK(void, __fastcall, CDatabaseLoaderLoadArchive, 0x69AB10,
	Hedgehog::Database::CDatabaseLoader* This,
	void* Edx,
	boost::shared_ptr<Hedgehog::Database::CDatabase> database, 
	const Hedgehog::Base::CSharedString& archiveListName,
	const Hedgehog::Database::SArchiveParam& archiveParam,
	bool unknown0, 
	bool unknown1)
{
	This->LoadDirectory(
		database,
		"work/" + archiveListName.substr(0, archiveListName.find('.')),
		{ archiveParam.Priority + 1, archiveParam.Flags });

	originalCDatabaseLoaderLoadArchive(
	    This,
		Edx,
		database,
		archiveListName,
		archiveParam,
		unknown0,
		unknown1);
}

namespace bb
{
	void InitWork()
	{
		INSTALL_HOOK(CDirectoryD3D9GetFiles);
		INSTALL_HOOK(CDatabaseLoaderLoadArchive);
		WRITE_MEMORY(0x6A6D0C, uint8_t, 0x90, 0xE9);
	}
}