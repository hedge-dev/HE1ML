#include "Globals.h"
#define BB_EXCLUDE_NAMESPACE_ALIASES
#include <BlueBlur.h>

static uint32_t archiveListMakeSerialMidAsmHookReturnAddr = 0x69C334;

static void __declspec(naked) archiveListMakeSerialMidAsmHook()
{
    __asm
	{
		mov ecx, [esp + 0x100 - 0xF8] // Name
		mov [eax + 0x28], ecx // Move to archive list

		// Original code
		push eax
		lea ecx, [esp + 0x104 - 0xF4]

		jmp[archiveListMakeSerialMidAsmHookReturnAddr]
	}
}

static uint32_t archiveListMakeAsyncMidAsmHookReturnAddr = 0x6A7DEC;

static void __declspec(naked) archiveListMakeAsyncMidAsmHook()
{
	__asm
	{
		// Name
		mov eax, [ebp + 0x4]
		add eax, [esp + 0x138 - 0x124]
		mov eax, [eax + 4]

		// Move to archive list
		mov [esi + 0x28], eax

		// Original code
		push esi
		lea ecx, [esp + 0x13C - 0x118]

		jmp[archiveListMakeAsyncMidAsmHookReturnAddr]
	}
}

HOOK(void, __fastcall, CDatabaseLoaderLoadArchiveList, 0x69B360,
	Hedgehog::Database::CDatabaseLoader* This,
	void* _,
	const boost::shared_ptr<Hedgehog::Database::CArchiveList>& archiveList,
	const uint8_t* data,
	uint32_t dataSize)
{
	originalCDatabaseLoaderLoadArchiveList(This, _, archiveList, data, dataSize);

	const auto arlName = *reinterpret_cast<const Hedgehog::Base::CSharedString*>(archiveList.get() + 1);

	const size_t sepIndex = arlName.find_last_of("\\/");
	const size_t dotIndex = arlName.find('.', sepIndex + 1);

	// Setup name (ARL name without extension)
	char name[0x400];
	const size_t nameSize = std::min(arlName.size(), dotIndex);

	strncpy(name, arlName.data(), nameSize);
	name[nameSize] = '\0';

	// Setup append ARL name
	char appendArlName[0x400];
	const size_t appendNameSize = nameSize + 1;

	strncpy(appendArlName, arlName.data(), sepIndex + 1);
	appendArlName[sepIndex + 1] = '+';
	strncpy(appendArlName + sepIndex + 2, arlName.data() + sepIndex + 1, std::min(arlName.size(), dotIndex - sepIndex - 1));
	strcpy(appendArlName + appendNameSize, ".arl");

	// Collect ARL files from mods
	const auto bindings = g_binder->CollectBindings(appendArlName);

	// Iterate in reverse so priority is still correct when loading splits in reverse.
	for (auto it = bindings.rbegin(); it != bindings.rend(); ++it)
	{
		const size_t curSplitCount = archiveList->m_ArchiveSizes.size();

		const auto buffer = std::unique_ptr<Buffer>{ read_file((*it).path.c_str(), false) };
		originalCDatabaseLoaderLoadArchiveList(This, _, archiveList, buffer->memory, buffer->size);

		char appendArPath[0x400];
		strcpy(appendArPath, (*it).path.c_str());

		for (size_t i = curSplitCount; i < archiveList->m_ArchiveSizes.size(); i++)
		{
			sprintf(name + nameSize, ".ar.%02d", i);
			sprintf(appendArPath + (*it).path.size() - 1, ".%02d", i - curSplitCount); // .arl -> .ar.%02d

			g_binder->BindFile(name, appendArPath, (*it).bind.priority);
		}
	}
}

static uint32_t arNameSprintfSerialMidAsmHookReturnAddr = 0x69ABDF;

static void __declspec(naked) arNameSprintfSerialMidAsmHook()
{
    __asm
	{
		mov eax, [esp + 0x144 - 0x128] // Split count
		sub eax, edi // Split index
		dec eax
		push eax

		push [ebx] // File path

		push 0x13E29B0 // %s.%02d

		lea eax, [esp + 0x150 - 0x104] // Destination
		push eax

		jmp[arNameSprintfSerialMidAsmHookReturnAddr]
	}
}

static uint32_t arNameSprintfAsyncMidAsmHookReturnAddr = 0x6A73F0;

static void __declspec(naked) arNameSprintfAsyncMidAsmHook()
{
	__asm
	{
		mov eax, [esp + 0x9E8 - 0x9C4] // Split count
		sub eax, [esp + 0x9E8 - 0x9CC] // Split index
		dec eax
		push eax

		// File path
		mov ecx, [esi + 0xC]
		add ecx, edi
		push [ecx]

		push 0x13E2F38 // %s.%02d

		lea ecx, [esp + 0x9F4 - 0x108] // Destination
		push ecx

		jmp[arNameSprintfAsyncMidAsmHookReturnAddr]
	}
}

HOOK(void, __fastcall, CDirectoryD3D9GetFiles, 0x667160,
	void* This,
	void*,
	hh::list<Hedgehog::Base::CSharedString>& list,
	Hedgehog::Base::CSharedString& path,
	void*,
	void*)
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

HOOK(void, __fastcall, CDatabaseLoaderLoadArchive, 0x69AB10,
	Hedgehog::Database::CDatabaseLoader* This,
	void* _,
	boost::shared_ptr<Hedgehog::Database::CDatabase> database,
	const Hedgehog::Base::CSharedString& archiveName,
	const Hedgehog::Database::SArchiveParam& archiveParam,
	bool loadAllData,
	bool loadImmediate)
{
	This->LoadDirectory(
		database, 
		"work/" + archiveName.substr(0, archiveName.find('.', archiveName.find_last_of("\\/") + 1)),
		archiveParam);

	originalCDatabaseLoaderLoadArchive(
		This,
		_,
		database,
		archiveName,
		archiveParam,
		loadAllData,
		loadImmediate);
}

namespace bb
{
	void InitWork()
	{
		// Increase CArchiveList memory size for char pointer
		WRITE_MEMORY(0x69C317, uint8_t, 0x2C); // Serial
		WRITE_MEMORY(0x6A7DCD, uint8_t, 0x2C); // Async

		// Store archive list name in extra space
		WRITE_JUMP(0x69C32F, archiveListMakeSerialMidAsmHook);
		WRITE_JUMP(0x6A7DE7, archiveListMakeAsyncMidAsmHook);

		// Load append ARLs and append ARs as extra splits to original AR
		INSTALL_HOOK(CDatabaseLoaderLoadArchiveList);

		// Load splits in reverse for correct priority
		WRITE_JUMP(0x69ABCC, arNameSprintfSerialMidAsmHook);
		WRITE_JUMP(0x6A73D3, arNameSprintfAsyncMidAsmHook);

		// Load bound files when loading work folder
		INSTALL_HOOK(CDirectoryD3D9GetFiles);

		// Load work folder for serial archives
		INSTALL_HOOK(CDatabaseLoaderLoadArchive);
	}
}