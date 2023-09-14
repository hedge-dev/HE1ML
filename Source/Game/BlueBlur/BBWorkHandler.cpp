#include "Globals.h"
#define BB_EXCLUDE_NAMESPACE_ALIASES
#include <BlueBlur.h>
#include <Hedgehog/Database/System/hhDecompressCAB.h>

std::mutex decompress_mtx{};
boost::scoped_ptr<Hedgehog::Database::CDecompressCAB> decompressor;

void DecompressCAB(boost::shared_ptr<uint8_t[]>& data, size_t& size)
{
	std::lock_guard lock(decompress_mtx);
	if (decompressor == nullptr)
	{
		decompressor.reset(new Hedgehog::Database::CDecompressCAB());
	}

	class MyCallback : public Hedgehog::Database::CCallback
	{
	public:
		boost::shared_ptr<uint8_t[]>& data;
		size_t& size;

		MyCallback(boost::shared_ptr<uint8_t[]>& result, size_t& result_size) : data(result), size(result_size)
		{

		}

		void Invoke(const boost::shared_ptr<Hedgehog::Database::CDatabase>& in_spDatabase, const Hedgehog::Base::CSharedString& in_rName, boost::shared_ptr<uint8_t[]> in_spData, size_t in_DataSize, const Hedgehog::Database::SArchiveParam& in_rParam) override
		{
			data = in_spData;
			size = in_DataSize;
		}
	};

	const Hedgehog::Database::SArchiveParam param{0, 0};
	
	auto cb = boost::make_shared<MyCallback>(data, size);

	decompressor->Decompress(nullptr, "", data, size, size, cb, param);
	decompressor->Update();
}

class ArchiveListEx : public Hedgehog::Database::CArchiveList
{
public:
	Hedgehog::Base::CSharedString name;
	uint32_t appendCount;
};

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
	const boost::shared_ptr<ArchiveListEx>& archiveList,
	const uint8_t* data,
	uint32_t dataSize)
{
	originalCDatabaseLoaderLoadArchiveList(This, _, archiveList, data, dataSize);

	const size_t originalSplitCount = archiveList->m_ArchiveSizes.size();

	const size_t sepIndex = archiveList->name.find_last_of("\\/");
	const size_t dotIndex = archiveList->name.find('.', sepIndex + 1);

	// Setup name (ARL name without extension)
	char name[0x400];
	const size_t nameSize = std::min(archiveList->name.size(), dotIndex);

	strncpy(name, archiveList->name.data(), nameSize);
	name[nameSize] = '\0';

	// Setup append ARL name
	char appendArlName[0x400];
	const size_t appendNameSize = nameSize + 1;

	strncpy(appendArlName, archiveList->name.data(), sepIndex + 1);
	appendArlName[sepIndex + 1] = '+';
	strncpy(appendArlName + sepIndex + 2, archiveList->name.data() + sepIndex + 1, std::min(archiveList->name.size(), dotIndex - sepIndex - 1));
	strcpy(appendArlName + appendNameSize, ".arl");

	// Collect ARL files from mods
	const auto bindings = g_binder->CollectBindings(appendArlName);

	// Iterate in reverse so priority is still correct when loading splits in reverse.
	for (auto it = bindings.rbegin(); it != bindings.rend(); ++it)
	{
		const size_t curSplitCount = archiveList->m_ArchiveSizes.size();

		const auto buffer = std::unique_ptr<Buffer>{ read_file((*it).path.c_str(), false) };
		if (buffer != nullptr)
		{
			auto shared_buffer = boost::shared_ptr<uint8_t[]>{ buffer->memory };
			auto buffer_size = buffer->size;
			buffer->memory = nullptr;

			DecompressCAB(shared_buffer, buffer_size);
			originalCDatabaseLoaderLoadArchiveList(This, _, archiveList, shared_buffer.get(), buffer_size);

			char appendArPath[0x400];
			strcpy(appendArPath, (*it).path.c_str());

			for (size_t i = curSplitCount; i < archiveList->m_ArchiveSizes.size(); i++)
			{
				sprintf(name + nameSize, ".ar.%02d", i);
				sprintf(appendArPath + (*it).path.size() - 1, ".%02d", archiveList->m_ArchiveSizes.size() - i - 1); // .arl -> .ar.%02d

				g_binder->BindFile(name, appendArPath, (*it).bind.priority);
			}
		}
	}

	archiveList->appendCount = archiveList->m_ArchiveSizes.size() - originalSplitCount; // Going to be used for the mid-asm hooks below
	archiveList->m_Loaded = true;
}

static uint32_t __stdcall transformSplitIndex(uint32_t index, ArchiveListEx* archiveList)
{
	return index < archiveList->appendCount ? (archiveList->m_ArchiveSizes.size() - index - 1) : (index - archiveList->appendCount);
}

static uint32_t arNameSprintfSerialMidAsmHookReturnAddr = 0x69ABDF;

static void __declspec(naked) arNameSprintfSerialMidAsmHook()
{
    __asm
	{
		// Split index
		push [esp + 0x144 - 0x118]
		push edi
		call transformSplitIndex
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
		// Split index
		mov ecx, [esp + 0x9E8 - 0x9C8]
		mov eax, [esi + 0x1D0]
		lea eax, [eax + ecx * 8]
		push [eax]
		push [esp + 0x9EC - 0x9CC]
		call transformSplitIndex
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

static void __fastcall loadHashArchiveMidAsmHook(Hedgehog::Base::CSharedString& workDirPath, void* _, uintptr_t pathHolder)
{
	new (&workDirPath) Hedgehog::Base::CSharedString("work/"); // needs to be constructed since that's what we are overriding
	workDirPath += *reinterpret_cast<const Hedgehog::Base::CSharedString*>(pathHolder + 0x20); // dir name
	workDirPath += "#";
	workDirPath += *reinterpret_cast<const Hedgehog::Base::CSharedString*>(pathHolder + 0x24); // file name
}

static uint32_t loadHashArchiveTrampolineReturnAddr = 0xD47CE7;

static void __declspec(naked) loadHashArchiveTrampoline()
{
    __asm
	{
		push esi
		call loadHashArchiveMidAsmHook
		jmp[loadHashArchiveTrampolineReturnAddr]
	}
}

HOOK(void, __cdecl, MakeVertexShaderDataV2, 0x742D20,
	Hedgehog::Mirage::CVertexShaderData* vertexShaderData,
	const uint8_t* data,
	const Hedgehog::Mirage::CMirageDatabaseWrapper& mirageDatabaseWrapper,
	Hedgehog::Mirage::CRenderingInfrastructure* renderingInfrastructure)
{
	if (!vertexShaderData->IsMadeOne())
		originalMakeVertexShaderDataV2(vertexShaderData, data, mirageDatabaseWrapper, renderingInfrastructure);
}

HOOK(void, __cdecl, MakeVertexShaderDataV1, 0x742CA0,
	Hedgehog::Mirage::CVertexShaderData* vertexShaderData,
	const uint8_t* data,
	const Hedgehog::Mirage::CMirageDatabaseWrapper& mirageDatabaseWrapper,
	Hedgehog::Mirage::CRenderingInfrastructure* renderingInfrastructure)
{
	if (!vertexShaderData->IsMadeOne())
		originalMakeVertexShaderDataV1(vertexShaderData, data, mirageDatabaseWrapper, renderingInfrastructure);
}

HOOK(void, __cdecl, MakeVertexShaderDataV0, 0x742C00,
	Hedgehog::Mirage::CVertexShaderData* vertexShaderData,
	const uint8_t* data,
	const Hedgehog::Mirage::CMirageDatabaseWrapper& mirageDatabaseWrapper,
	Hedgehog::Mirage::CRenderingInfrastructure* renderingInfrastructure)
{
	if (!vertexShaderData->IsMadeOne())
		originalMakeVertexShaderDataV0(vertexShaderData, data, mirageDatabaseWrapper, renderingInfrastructure);
}

HOOK(void, __stdcall, MakeVertexShaderCodeData, 0x724740,
	Hedgehog::Mirage::CVertexShaderCodeData* vertexShaderCodeData,
	const uint8_t* data,
	size_t dataSize,
	Hedgehog::Mirage::CRenderingInfrastructure* renderingInfrastructure)
{
	if (!vertexShaderCodeData->IsMadeOne())
		originalMakeVertexShaderCodeData(vertexShaderCodeData, data, dataSize, renderingInfrastructure);
}

HOOK(void, __stdcall, MakePixelShaderCodeData, 0x724610,
	Hedgehog::Mirage::CPixelShaderCodeData* pixelShaderCodeData,
	const uint8_t* data,
	size_t dataSize,
	Hedgehog::Mirage::CRenderingInfrastructure* renderingInfrastructure)
{
	if (!pixelShaderCodeData->IsMadeOne())
		originalMakePixelShaderCodeData(pixelShaderCodeData, data, dataSize, renderingInfrastructure);
}

namespace bb
{
	void InitWork()
	{
		// Increase CArchiveList memory size for char pointer
		WRITE_MEMORY(0x69C317, uint8_t, sizeof(ArchiveListEx)); // Serial
		WRITE_MEMORY(0x6A7DCD, uint8_t, sizeof(ArchiveListEx)); // Async

		// Store archive list name in extra space
		WRITE_JUMP(0x69C32F, archiveListMakeSerialMidAsmHook);
		WRITE_JUMP(0x6A7DE7, archiveListMakeAsyncMidAsmHook);

		// Stop LoadArchiveList from setting Loaded to true, we'll do it ourselves
		WRITE_NOP(0x0069B6E0, 0x04);

		// Load append ARLs and append ARs as extra splits to original AR
		INSTALL_HOOK(CDatabaseLoaderLoadArchiveList);

		// Load splits in reverse for correct priority
		WRITE_JUMP(0x69ABCC, arNameSprintfSerialMidAsmHook);
		WRITE_JUMP(0x6A73D3, arNameSprintfAsyncMidAsmHook);

		// Load bound files when loading work folder
		INSTALL_HOOK(CDirectoryD3D9GetFiles);

		// Load work folder for serial archives
		INSTALL_HOOK(CDatabaseLoaderLoadArchive);

		// Load work folder for async # archives
		WRITE_JUMP(0xD47CE2, loadHashArchiveTrampoline);

		// Fix database data make functions that don't check for IsMadeOne
		// and cause priority to be reversed due to always overriding data
		INSTALL_HOOK(MakeVertexShaderDataV2);
		INSTALL_HOOK(MakeVertexShaderDataV1);
		INSTALL_HOOK(MakeVertexShaderDataV0);
		INSTALL_HOOK(MakeVertexShaderCodeData);
		INSTALL_HOOK(MakePixelShaderCodeData);
	}
}