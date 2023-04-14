#include "Globals.h"
#include "API/hhList.h"
#include "API/hhSharedString.h"

namespace hh = Hedgehog;
using namespace hh::Base;

HOOK(void, __fastcall, CDirectoryD3D9_GetFiles, 0x00667160, void* ecx, void* edx, hh::list<CSharedString>& list, CSharedString& path, void* a3, void* a4)
{
	g_binder->EnumerateFiles(path.c_str(), [&list](auto& x) -> bool
		{
			list.push_back(x.filename().string().c_str());
			return true;
		});
}

//HOOK(void, __fastcall, CDatabaseLoaderLoadArchive, 0x69AB10,
//	void* This, void* Edx, std::shared_ptr<void*> in_spDatabase, const Hedgehog::Base::CSharedString& in_rArchiveListName,
//	const void* in_rArchiveParam, bool in_Unknown0, bool in_Unknown1)
//{
//	originalCDatabaseLoaderLoadArchive(This, Edx, in_spDatabase, in_rArchiveListName, in_rArchiveParam, in_Unknown0, in_Unknown1);
//}

namespace bb
{
	void InitWork()
	{
		INSTALL_HOOK(CDirectoryD3D9_GetFiles);
		// INSTALL_HOOK(CDatabaseLoaderLoadArchive);
	}
}