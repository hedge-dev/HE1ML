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
	const auto binds = g_binder->CollectBindings(path.c_str());
	for (const auto& bind : binds)
	{
		for (const auto& entry : std::filesystem::directory_iterator(bind.path))
		{
			if (entry.is_regular_file())
			{
				list.push_back(entry.path().filename().string().c_str());
			}
		}
	}
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

			const auto bindings = g_binder->CollectBindings(arlName.c_str());

			This->LoadDirectory(database, "work/" + archiveName.substr(0, extensionIdx), archiveParam);
			for (const auto& bind : bindings)
			{
				auto symbol = make_string_symbol(path_dirname(bind.path.c_str()));
				const auto arEncoded = std::format("M{{{}}}{}", ptrtostr(reinterpret_cast<size_t>(symbol)), arName.c_str());
				const auto arlEncoded = std::format("M{{{}}}{}", ptrtostr(reinterpret_cast<size_t>(symbol)), path_noextension(path_filename(arlName.c_str())));

				if (!This->m_spArchiveListManager->GetArchiveList(arlEncoded.c_str()))
				{
					const auto file = std::unique_ptr<Buffer>{ read_file(bind.path.c_str(), false) };
					if (file)
					{
						auto archiveList = boost::make_shared<Hedgehog::Database::CArchiveList>();
						This->m_spArchiveListManager->AddArchiveList(arlEncoded.c_str(), archiveList);
						This->LoadArchiveList(archiveList, file->memory, file->size);
					}
				}

				This->LoadArchive(database, arEncoded.c_str(), archiveParam, false, true);
			}
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