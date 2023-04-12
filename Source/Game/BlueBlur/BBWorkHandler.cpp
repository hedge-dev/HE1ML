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

namespace bb
{
	void InitWork()
	{
		INSTALL_HOOK(CDirectoryD3D9_GetFiles);
	}
}