#include "Globals.h"

#define BB_EXCLUDE_NAMESPACE_ALIASES
#include <BlueBlur.h>

#include "CRIWARE/Criware.h"

HOOK(void, __fastcall, CDirectoryD3D9GetFiles, 0x667160,
     void* This,
     void* Edx,
     hh::list<Hedgehog::Base::CSharedString>& list,
     Hedgehog::Base::CSharedString& path,
     void* a3,
     void* a4)
{
	g_binder->EnumerateFiles(path.c_str(), [&list, &path](auto& x) -> bool
	{
		list.emplace_back(x.filename().string().c_str());
		return true;
	});
}

HOOK(boost::shared_ptr<Hedgehog::Database::SLoadElement>*, __fastcall, CDatabaseLoaderLoadArchive, 0x69A850,
    Hedgehog::Database::CDatabaseLoader* This,
    void* Edx,
	boost::shared_ptr<Hedgehog::Database::SLoadElement>& loadElement,
	boost::shared_ptr<Hedgehog::Database::CDatabase> database,
	const Hedgehog::Base::CSharedString& archiveName,
	const Hedgehog::Database::SArchiveParam& archiveParam,
	bool loadImmediate)
{
	const size_t extensionIdx = archiveName.find(".ar.00");
	if (extensionIdx != Hedgehog::Base::CSharedString::npos)
	{
		const size_t separatorIdx = archiveName.find_last_of("\\/");
		if (archiveName.data()[separatorIdx + 1] != '+')
		{
			const auto directoryPath = archiveName.substr(0, separatorIdx + 1);
			const auto fileName = archiveName.substr(separatorIdx + 1, extensionIdx - separatorIdx - 1);

			const auto name = directoryPath + "+" + fileName;
			const auto arName = name + ".ar";
			const auto arlName = arName + "l";

			std::string filePath;
			if (g_binder->ResolvePath(arlName.c_str(), &filePath) == eBindError_None)
			{
				auto archiveList = boost::make_shared<Hedgehog::Database::CArchiveList>();

				FILE* file = fopen(filePath.c_str(), "rb");
				if (file)
				{
					fseek(file, 0, SEEK_END);
					size_t fileSize = ftell(file);
					fseek(file, 0, SEEK_SET);

					auto fileData = std::make_unique<uint8_t[]>(fileSize);
					fread(fileData.get(), 1, fileSize, file);
					fclose(file);

					This->m_spArchiveListManager->AddArchiveList(name, archiveList);
					This->LoadArchiveList(archiveList, fileData.get(), fileSize);

					This->LoadArchive(database, arName, archiveParam, false, true);
				}
			}

			This->LoadDirectory(database, "work/" + archiveName.substr(0, extensionIdx), archiveParam);
		}
	}
	return originalCDatabaseLoaderLoadArchive(This, Edx, loadElement, database, archiveName, archiveParam, loadImmediate);
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