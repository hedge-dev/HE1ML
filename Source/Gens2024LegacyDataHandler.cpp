#include <string_view>
#include <exception> // TODO: Remove me
#include <boost/function.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/shared_ptr.hpp>
#include <rad/rad_memory_stream.h>
#include <rad/rad_file.h> // TODO
#include <rad/rad_path.h> // TODO
#include <hedgelib/hh_new/hl_hh_packed_file_info.h> 
#include "Gens2024LegacyAudioUpgrader.h"
#include "Gens2024LegacyAudioPatcher.h"
#include "CRIWARE/Criware.h"
#include "Globals.h"
#include <fdi.h>

using namespace boost::placeholders;

// TODO: SIGSCAN OR REPLACE
FUNCTION_PTR(void*, __cdecl, __HH_ALLOC2, 0x1403118b0, size_t size);

// TODO: SIGSCAN OR REPLACE
FUNCTION_PTR(void, __cdecl, __HH_FREE, 0x1403119c0, void* ptr);

// TODO: SIGSCAN OR REPLACE
FUNCTION_PTR(void*, __cdecl, __HH_ALLOCALIGN, 0x140311930, size_t size, size_t alignment);

namespace Hedgehog::Base
{
	struct SStringHolder
	{
		union
		{
			struct
			{
				uint16_t RefCount;
				uint16_t Length;
			};

			uint32_t RefCountAndLength;
		};

		char aStr[1u];

		static inline SStringHolder* GetHolder(const char* in_pStr)
		{
			return (SStringHolder*)((size_t)in_pStr - 4);
		}

		inline void Release()
		{
			if ((uint16_t)InterlockedDecrement(&RefCountAndLength) == 0)
				__HH_FREE(this);
		}
	};

	class CSharedString;

	FUNCTION_PTR(size_t, __cdecl, fpGetHash, 0x14030fd10, // TODO: SIGSCAN
		CSharedString* thisPtr);

	class CSharedString
	{
	public:
		const char* data;

		inline SStringHolder* GetHolder() const
		{
			return SStringHolder::GetHolder(data);
		}

		inline size_t GetHash()
		{
			return fpGetHash(this);
		}

		CSharedString() = default; // TODO

		CSharedString(std::string_view str)
		{
			const auto pHolder = static_cast<SStringHolder*>(
				__HH_ALLOC2(str.size() + 0x14U & 0xfffffff0)
			);

			pHolder->RefCount = 1;
			pHolder->Length = static_cast<uint16_t>(str.size());

			memcpy(pHolder->aStr, str.data(), str.size() + 1);

			data = pHolder->aStr;
		}

		~CSharedString()
		{
			GetHolder()->Release();
		}
	};
}

namespace Hedgehog::Database
{
	class CArchiveDatabaseLoader;
	class CDatabase;
}

// TODO: What is the actual name of this?
struct TypeInfo
{
	void* creatorFunc;
	int unk1;
	int paramCount;
};

// TODO: What is the actual name of this?
struct HashedString
{
	Hedgehog::Base::CSharedString str;
	size_t hash;
};

struct Unknown1
{
	int a;
	int b;
	HashedString* c;
};

namespace Hedgehog::Database
{
	enum EDatabaseDataFlags : uint8_t
	{
		eDatabaseDataFlags_IsMadeOne = 0x1,
		eDatabaseDataFlags_IsMadeAll = 0x2,
		eDatabaseDataFlags_CreatedFromArchive = 0x4,
		eDatabaseDataFlags_IsMadeMakingOne = 0x8
	};

	class CDatabaseData;

	FUNCTION_PTR(CDatabaseData*, __cdecl, fpConstructCDatabaseData, 0x14031ee50, // TODO: SIGSCAN address!!!
		CDatabaseData* thisPtr);

	class CDatabaseData
	{
	public:
		uint8_t m_Flags;
		Base::CSharedString m_TypeAndName;

		virtual ~CDatabaseData() = default;

		virtual bool CheckMadeAll()
		{
			return true;
		}

		inline bool IsMadeOne() const
		{
			return (m_Flags & eDatabaseDataFlags_IsMadeOne) != 0;
		}
		
		inline void SetMadeOne()
		{
			m_Flags |= eDatabaseDataFlags_IsMadeOne;
		}

		inline void SetCreatedFromArchive()
		{
			m_Flags |= eDatabaseDataFlags_CreatedFromArchive;
		}

		CDatabaseData()
		{
			fpConstructCDatabaseData(this);
		}
	};

	class CDatabase;

	FUNCTION_PTR(void, __cdecl, fpCreateData, 0x140326310, // TODO: SIGSCAN address!!!
		CDatabase* thisPtr, Base::CSharedString& name);

	FUNCTION_PTR(void, __cdecl, fpGetOrCreateData, 0x140319e10, // TODO: SIGSCAN address!!!
		Hedgehog::Database::CDatabase* thisPtr,
		boost::shared_ptr<Hedgehog::Database::CDatabaseData>& out,
		TypeInfo& param_2,
		Unknown1& param_3,
		unsigned long long param_4);

	class CDatabase
	{
	public:
		void CreateData(Base::CSharedString& name)
		{
			fpCreateData(this, name);
		}

		boost::shared_ptr<CDatabaseData> GetOrCreateData(
			TypeInfo& param_2,
			Unknown1& param_3,
			unsigned long long param_4)
		{
			boost::shared_ptr<CDatabaseData> out;
			fpGetOrCreateData(this, out, param_2, param_3, param_4);
			return out;
		}
	};
}

namespace Hedgehog::Sound
{
	class CSoundCueSheetMemoryData;

	FUNCTION_PTR(boost::shared_ptr<CSoundCueSheetMemoryData>*,
		__cdecl, fpCreateCSoundCueSheetMemoryData, 0x148cc6b10, // TODO: SIGSCAN
		boost::shared_ptr<CSoundCueSheetMemoryData>& out,
		const Base::CSharedString& name,
		const void* acbData,
		long acbDataSize
	);

	class CSoundCueSheetMemoryData
	{
	public:
		Base::CSharedString name;
		void* cueSheet;

		static boost::shared_ptr<CSoundCueSheetMemoryData> Create(
			const Base::CSharedString& name, const void* acbData, long acbDataSize)
		{
			boost::shared_ptr<CSoundCueSheetMemoryData> out;
			fpCreateCSoundCueSheetMemoryData(out, name, acbData, acbDataSize);
			return out;
		}
	};

	class CCueSheetBinaryData
		: public Database::CDatabaseData
	{
	public:
		boost::shared_ptr<CSoundCueSheetMemoryData> data;

		static CCueSheetBinaryData* Create()
		{
			const auto ptr = new CCueSheetBinaryData();
			ptr->SetCreatedFromArchive();

			return ptr;
		}

		bool CreateCueSheet(
			const Base::CSharedString& name,
			const void* acbData,
			unsigned long acbDataSize)
		{
			data = CSoundCueSheetMemoryData::Create(
				name,
				acbData,
				acbDataSize
			);

			return true;
		}

		CCueSheetBinaryData() = default;
	};
}

namespace Hedgehog::Mirage
{
	class CRenderingInfrastructure
	{
		// TODO
	};

	class CPhysicalAllocator
	{
		// TODO
	};
}

namespace gens2024
{
	using TypeMakeFuncPtr = void(*)(
		const Hedgehog::Base::CSharedString&,
		void*,
		unsigned long,
		boost::shared_ptr<Hedgehog::Database::CDatabase>&
	);

	using TypeMakeFunc = boost::function<void(
		const Hedgehog::Base::CSharedString&,
		void*,
		unsigned long,
		boost::shared_ptr<Hedgehog::Database::CDatabase>&
	)>;

	using TypeCreateFunc = boost::function<void(
		Hedgehog::Database::CDatabase&,
		const Hedgehog::Base::CSharedString&
	)>;

	static constexpr hl::cri::atom::packed_version acbWriteVersion = { 1, 32, 00 };

	FUNCTION_PTR(Hedgehog::Base::CSharedString, __cdecl, BuildSoundNameString, 0x148f45770, // TODO: SIGSCAN THIS!!!
		const Hedgehog::Base::CSharedString& param_2,
		const Hedgehog::Base::CSharedString& param_3);

	static void CreateCSBParser(
		Hedgehog::Database::CDatabase& db,
		const Hedgehog::Base::CSharedString& name)
	{
		//LOG("CREATE TIME %s", name.data);

		auto soundNameStr = BuildSoundNameString(
			Hedgehog::Base::CSharedString("sound-cont"),
			name
		);

		db.CreateData(soundNameStr);
	}

	static void MakeCSB(
		const Hedgehog::Base::CSharedString& name,
		void* data,
		unsigned long dataSize,
		boost::shared_ptr<Hedgehog::Database::CDatabase>& db)
	{
		// NOTE: In practice, this function should only ever be called for csb
		// files within mod ar files. The redirection has already happened.

		LOG("Upgrading 2011 CueSheet \"%s.csb\"", name.data);

		auto soundNameStr = BuildSoundNameString(
			Hedgehog::Base::CSharedString("sound-cont"),
			name
		);

		//auto dbData = db->GetSoundData(soundNameStr, 0);
		const auto hash = soundNameStr.GetHash();

		TypeInfo typeInfo;
		typeInfo.creatorFunc = &Hedgehog::Sound::CCueSheetBinaryData::Create;
		typeInfo.unk1 = 0;
		typeInfo.paramCount = 1;

		HashedString soundNameHashedStr;
		soundNameHashedStr.str = soundNameStr;
		soundNameHashedStr.hash = hash;

		Unknown1 unk1;
		unk1.a = -1;
		unk1.b = -1;
		unk1.c = &soundNameHashedStr;

		auto dbData = db->GetOrCreateData(typeInfo, unk1, 0);

		if (!data || !dbData)
		{
			return;
		}

		if (!dbData->IsMadeOne())
		{
			try
			{
				hl::cri::atom::cue_sheet acb(name.data); // TODO: Also pass ACB GUID (get it from a built-in mapping between names and GUIDS?) and allocator
				TryUpgradeCSB(acb, data, dataSize); // TODO: Pass in tmpAllocator and csbAllocator

				rad::memory_stream acbMemStream; // TODO: Pass allocator
				acb.write(acbMemStream, { acbWriteVersion }); // TODO: Pass allocator

				const auto acbData = acbMemStream.data();
			const auto cueSheetBinaryData = static_cast<Hedgehog::Sound::CCueSheetBinaryData*>(dbData.get());

			cueSheetBinaryData->CreateCueSheet(
				name,
				acbData.data(),
				static_cast<unsigned long>(acbData.size())
			);
			}
			catch (const std::exception& ex)
			{
				g_loader->WriteLog(
					ML_LOG_LEVEL_ERROR,
					ML_LOG_CATEGORY_GENERAL,
					"Failed to upgrade 2011 CueSheet \"%s\": \"%s\"\n",
					reinterpret_cast<size_t>(name.data),
					reinterpret_cast<size_t>(ex.what()),
					nullptr
				);
			}
			catch (...)
			{
				g_loader->WriteLog(
					ML_LOG_LEVEL_ERROR,
					ML_LOG_CATEGORY_GENERAL,
					"Failed to upgrade 2011 CueSheet \"%s\": \"%s\"\n",
					reinterpret_cast<size_t>(name.data),
					reinterpret_cast<size_t>("[NO ERROR MESSAGE]"),
					nullptr
				);
			}

			dbData->SetMadeOne();
		}
	}

	HOOK(void, __cdecl, MakePFI, nullptr,
		const Hedgehog::Base::CSharedString& name,
		void* data,
		unsigned long dataSize,
		boost::shared_ptr<Hedgehog::Database::CDatabase>& db)
	{
		if (data)
		{
			try
			{
				rad::readonly_memory_stream inStream(data, dataSize);
				hl::hh_new::mirage::packed_file_info pfi;
				const auto fileInfo = pfi.read(inStream);

				if (fileInfo.offsetType == hl::hh_new::mirage::off_type::u32)
				{
					LOG("Upgrading 32-bit HH data \"%s.pfi\"", name.data);

					rad::memory_stream outStream;
					pfi.write(outStream, { .offsetType = hl::hh_new::mirage::off_type::u64 });

					originalMakePFI(
						name,
						outStream.data().data(),
						static_cast<unsigned long>(outStream.data().size()),
						db
					);

					return;
				}
			}
			catch (const std::exception& ex)
			{
				g_loader->WriteLog(
					ML_LOG_LEVEL_ERROR,
					ML_LOG_CATEGORY_GENERAL,
					"Failed to upgrade PFI \"%s\": \"%s\"\n",
					reinterpret_cast<size_t>(name.data),
					reinterpret_cast<size_t>(ex.what()),
					nullptr
				);
			}
		}

		originalMakePFI(name, data, dataSize, db);
	}

	HOOK(void, __cdecl, RegisterType, nullptr,
		Hedgehog::Database::CDatabase* thisPtr, Hedgehog::Base::CSharedString& name,
		TypeMakeFunc makeFunc, TypeCreateFunc createFunc)
	{
		const std::string_view typeName(name.data);

		if (typeName == "pfi")
		{
			const auto makeFuncAddr = reinterpret_cast<const uintptr_t*>(&makeFunc)[1];
			INSTALL_HOOK_ADDRESS(MakePFI, makeFuncAddr);
		}
		else if (typeName == "acb")
		{
			// Register csb type IN ADDITION to acb type.
			Hedgehog::Base::CSharedString csbName("csb");

			LOG("Register type: %s", csbName.data);
			originalRegisterType(
				thisPtr,
				csbName,
				boost::bind(MakeCSB, _1, _2, _3, _4),
				boost::bind(CreateCSBParser, _1, _2)
			);
		}

		LOG("Register type: %s", name.data);
		originalRegisterType(thisPtr, name, makeFunc, createFunc);
	}

	struct CriAtomPlayerTag
	{
		INSERT_PADDING(0x270);
		uint64_t offset;
		uint32_t size;
	};

	struct AFS2Header
	{
		uint32_t signature;
		uint8_t version;
		uint8_t dataPosSize;
		uint16_t idAlignment;
		uint32_t waveformCount;
		uint16_t dataAlignment;
		uint16_t subkey;

		inline const uint16_t* ids() const noexcept
		{
			return reinterpret_cast<const uint16_t*>(this + 1);
		}

		inline const uint32_t* dataPositions() const noexcept
		{
			return reinterpret_cast<const uint32_t*>(ids() + waveformCount);
		}

		inline const char* extraHE1MLStrings() const noexcept
		{
			return reinterpret_cast<const char*>(dataPositions() + waveformCount + 1);
		}

		unsigned long getWaveformIndex(uint16_t id) const
		{
			const auto idsPtr = ids();

			for (unsigned long i = 0; i < waveformCount; ++i)
			{
				if (idsPtr[i] == id)
				{
					return i;
				}
			}

			return waveformCount;
		}

		const HE1MLExtraWaveformData* getExtraHE1MLData(uint16_t id) const
		{
			const auto i = getWaveformIndex(id);
			if (i == waveformCount)
			{
				return nullptr;
			}

			const auto dataPos = dataPositions()[i];

			return reinterpret_cast<const HE1MLExtraWaveformData*>(
				reinterpret_cast<const unsigned char*>(this) + dataPos
			);
		}
	};

	struct CriAtomAwbTag
	{
		void* unknown1;
		AFS2Header* memoryAwb;
		const char* path;
		void* unknown3;
		void* unknown4;
		void* unknown5;
		void* unknown6;
		void* unknown7;
		AFS2Header* streamingAwbToc;
	};

	HOOK(void, CRIAPI, criatomplayer_set_wave_id_core, nullptr,
		CriAtomPlayerTag* player, CriAtomAwbTag* awb, CriSint32 id) //, CriUint32 offset)
	{
		const auto path = (awb->path) ? awb->path : "[NO PATH]";

		if (awb->streamingAwbToc)
		{
			if (awb->streamingAwbToc->subkey == 67)
			{
				const auto extraData = awb->streamingAwbToc->getExtraHE1MLData(id);

				if (extraData)
				{
					const auto extraStrings = awb->streamingAwbToc->extraHE1MLStrings();
					const auto redirectPath = extraStrings + extraData->streamingPathOff;

					LOG("USING FAKE AWB for id %d", (void*)(size_t)id);
					LOG("override with: \"%s\"", redirectPath);
					LOG("data offset: %d, size: %d", (void*)extraData->dataOffset, (void*)extraData->dataSize);

					g_cri->criatomplayer_set_file_core(
						player,
						nullptr,
						redirectPath,
						extraData->dataOffset,
						extraData->dataSize
					);

					player->offset = extraData->dataOffset;
					player->size = extraData->dataSize;

					return;
				}

				LOG("FAKE AWB FOUND FOR \"%s\", BUT MISSING HE1ML EXTRA DATA FOR ID %d !!!", path, (void*)(size_t)id);
			}
		}

		originalcriatomplayer_set_wave_id_core(player, awb, id);
	}

	static rad::vector<rad::memory_stream> acb_data_array; // TODO

	HOOK(CriAtomExAcbHn, CRIAPI, criAtomExAcb_LoadAcbFile, nullptr,
		CriFsBinderHn acb_binder, const CriChar8* acb_path,
		CriFsBinderHn awb_binder, const CriChar8* awb_path,
		void* work, CriSint32 work_size)
	{
		// NOTE: In practice, this hook should only ever be called for "streaming" acb files.
		// NOT for acb files contained within ar files.

		assert(acb_path &&
			"criAtomExAcb_LoadAcbFile should never be called with a NULL acb_path"
		);

		std::string replaceCueSheetPath, replaceStreamingPath;

		// 1: If a mod awb or acb is present, just redirect those files.
		// Ignore any csb or cpk mod files, in this case.
		{
			bool doSimple2024Redirect = false;

			if (g_loader->binder->ResolvePath(acb_path,
				&replaceCueSheetPath) == eBindError_None)
			{
				acb_path = replaceCueSheetPath.c_str();
				doSimple2024Redirect = true;
			}
			else if (awb_path && g_loader->binder->ResolvePath(
				awb_path, &replaceStreamingPath) == eBindError_None)
			{
				awb_path = replaceStreamingPath.c_str();
				doSimple2024Redirect = true;
			}

			if (doSimple2024Redirect)
			{
				LOG("Simple Redirect to: \"%s\" and \"%s\"", acb_path, awb_path);

				return originalcriAtomExAcb_LoadAcbFile(
					acb_binder,
					acb_path,
					awb_binder,
					awb_path,
					work,
					work_size
				);
			}
		}

		// 2: If a csb file is present, create an acb from it
		// ("upgrade" the csb to an acb) in-memory, and pass
		// it to the game.
		const char* errorFmtMessage = "Failed to collect info for CueSheet \"%s\": \"%s\"\n";
		const char* errorPath = acb_path;

		try
		{
			std::unique_ptr<CPKStreamer> cpkStreamer;
			SoundElementStreamingInfo streamingInfo = {
				.cleanAwbPath = awb_path
			};

			// Collect streaming info (a CPK and/or streaming directory)
			{
				assert(std::string_view{awb_path}.ends_with(".awb"));

				// HACK: Temporarily modify the const awb_path data, then modify it back.
				const auto awbExtPtr = (const_cast<char*>(awb_path) + std::strlen(awb_path)) - 4;

				std::memcpy(awbExtPtr, ".cpk", 4);

				if (g_loader->binder->ResolvePath(awb_path,
					&replaceStreamingPath) == eBindError_None) // .cpk
				{
					LOG("CPK redirect to \"%s\"", replaceStreamingPath.c_str());
					cpkStreamer.reset(new CPKStreamer(replaceStreamingPath.c_str()));
					streamingInfo.redirectCPK = cpkStreamer.get();
				}

				*awbExtPtr = '\0';

				if (g_loader->binder->ResolvePath(awb_path,
					&replaceStreamingPath) == eBindError_None) // directory
			{
					LOG("Directory redirect to: \"%s\"", replaceStreamingPath.c_str());
					streamingInfo.redirectDir = replaceStreamingPath;
			}

				std::memcpy(awbExtPtr, ".awb", 4);
			}

			// If a csb is found, upgrade it to acb and use it.
		{
				assert(std::string_view{acb_path}.ends_with(".acb"));

				// HACK: Temporarily modify the const acb_path data, then modify it back.
				const auto acbExtPtr = (const_cast<char*>(acb_path) + std::strlen(acb_path)) - 4;

			std::memcpy(acbExtPtr, ".csb", 4);

				const auto r = g_loader->binder->ResolvePath(
					acb_path,
					&replaceCueSheetPath
			);

			std::memcpy(acbExtPtr, ".acb", 4);

			if (r == eBindError_None)
			{
					errorFmtMessage = "Failed to upgrade 2011 CueSheet \"%s\": \"%s\"\n";
					errorPath = replaceCueSheetPath.c_str();
					LOG("Upgrading 2011 CueSheet \"%s\"", replaceCueSheetPath.c_str());

				rad::file_stream stream(
						replaceCueSheetPath.c_str(),
					rad::file_stream::OPEN_MODE_READ_ONLY |
					rad::file_stream::OPEN_HINT_SEQUENTIAL_ACCESS // TODO: Should this be random access?
				);

				const auto csbDataSize = static_cast<unsigned long>(stream.get_size());
				std::unique_ptr<unsigned char[]> csbData(new unsigned char[csbDataSize]);

				stream.read(csbData.get(), csbDataSize);

					const auto name = rad::path::get_stem(acb_path);
					hl::cri::atom::cue_sheet acb(name); // TODO: Also pass ACB GUID (get it from a built-in mapping between names and GUIDS?) and allocator
					TryUpgradeCSB(acb, csbData.get(), csbDataSize, &streamingInfo); // TODO: Pass in tmpAllocator and csbAllocator

					rad::memory_stream acbMemStream; // TODO: Pass allocator
					acb.write(acbMemStream, { acbWriteVersion }); // TODO: Pass allocator

					const auto acbData = acb_data_array.emplace_back(std::move(acbMemStream)).data();
					// TODO: Free the acbData later when the game attempts to release it !!!

					const auto hnd = g_cri->criAtomExAcb_LoadAcbData(
						acbData.data(),
						static_cast<CriSint32>(acbData.size()),
						awb_binder,
						awb_path,
						nullptr,
						0
					);

					if (!hnd)
					{
						LOG("Failed to load upgraded ACB data for \"%s\"", awb_path);
					}

					return hnd;
				}
			}

			// Otherwise, if there is any streaming data (a CPK or a streaming directory),
			// patch the clean ACB to utilize it instead.
			if (streamingInfo.redirectDir || streamingInfo.redirectCPK)
			{
				errorFmtMessage = "Failed to patch 2024 CueSheet \"%s\": \"%s\"\n";
				LOG("Patching 2024 CueSheet to use 2011 streaming data \"%s\"", acb_path);

				rad::file_stream stream(
					acb_path,
					rad::file_stream::OPEN_MODE_READ_ONLY | rad::file_stream::OPEN_FLAG_SHARED //|
				);

				hl::cri::atom::cue_sheet acb(stream);

				PatchACB(acb, streamingInfo);

				rad::memory_stream acbMemStream; // TODO: Pass allocator
				acb.write(acbMemStream, { acbWriteVersion }); // TODO: Pass allocator

				const auto acbData = acb_data_array.emplace_back(std::move(acbMemStream)).data();
				// TODO: Free the acbData later when the game attempts to release it !!!

				const auto hnd = g_cri->criAtomExAcb_LoadAcbData(
					acbData.data(),
					static_cast<CriSint32>(acbData.size()),
					awb_binder,
					awb_path,
					nullptr,
					0
				);

				if (!hnd)
			{
					LOG("Failed to load patched ACB data for \"%s\"", awb_path);
				}

				return hnd;
			}
		}
		catch (const std::exception& ex)
		{
			g_loader->WriteLog(
				ML_LOG_LEVEL_ERROR,
				ML_LOG_CATEGORY_GENERAL,
				errorFmtMessage,
				reinterpret_cast<size_t>(errorPath),
				reinterpret_cast<size_t>(ex.what()),
				nullptr
			);
		}
		catch (...)
		{
			g_loader->WriteLog(
				ML_LOG_LEVEL_ERROR,
				ML_LOG_CATEGORY_GENERAL,
				errorFmtMessage,
				reinterpret_cast<size_t>(errorPath),
				reinterpret_cast<size_t>("[NO ERROR MESSAGE]"),
				nullptr
			);
		}

		return originalcriAtomExAcb_LoadAcbFile(acb_binder, acb_path, awb_binder, awb_path, work, work_size);
	}

	// TODO: Move all of this archive/compression stuff to another file?
	FNALLOC(fdiAlloc)
	{
		return ::operator new(cb);
	}

	FNFREE(fdiFree)
	{
		return ::operator delete(pv);
	}

	FNOPEN(fdiOpen)
	{
		// HACK: Taken from LibGens. Since the FDI interface doesn't allow us to pass
		// arbitrary user data to the fdiOpen callback, our source stream is converted
		// to a hex string and passed as the CAB path.
		rad::stream* compressedAR;
		std::sscanf(pszFile, "%p", &compressedAR);
		
		return (INT_PTR)compressedAR;
	}

	FNREAD(fdiRead)
	{
		const auto stream = (rad::stream*)hf;
		return static_cast<UINT>(stream->try_read(pv, cb));
	}

	FNWRITE(fdiWrite)
	{
		const auto stream = (rad::stream*)hf;
		return static_cast<UINT>(stream->try_write(pv, cb));
	}

	FNCLOSE(fdiClose)
	{
		return 0;
	}

	FNSEEK(fdiSeek)
	{
		const auto stream = (rad::stream*)hf;
		stream->seek(static_cast<rad::stream::seek_mode>(seektype), dist);
		return static_cast<long>(stream->tell());
	}

	FNFDINOTIFY(fdiNotify)
	{
		return (fdint == fdintCOPY_FILE) ? (INT_PTR)pfdin->pv : 0;
	}

	class archive_allocator
		: public rad::allocator
	{
	public:
		void* allocate(std::size_t size, std::size_t alignment) override
		{
			return new uint8_t[size];
		}

		void* reallocate(
			void* ptr,
			std::size_t oldSize,
			std::size_t newSize,
			std::size_t alignment) override
		{
			const auto newPtr = new uint8_t[newSize];

			if (ptr)
			{
				std::memcpy(newPtr, ptr, newSize > oldSize ? oldSize : newSize);
				this->free(ptr);
			}

			return newPtr;
		}

		void free(void* ptr) noexcept override
		{
			delete[] static_cast<uint8_t*>(ptr);
		}
	};

	HOOK(void, __fastcall, CArchiveDatabaseLoaderLoadArchive, nullptr,
		Hedgehog::Database::CArchiveDatabaseLoader* This,
		const boost::shared_ptr<Hedgehog::Database::CDatabase>& in_spDatabase,
		boost::shared_ptr<uint8_t[]> in_spData,
		uint32_t in_DataSize,
		uint32_t in_DataSize1,
		void* in_pFileReader)
	{
		constexpr uint32_t minValidCABHeaderSize = 36;
		constexpr uint32_t CABSignature = 0x4643534DU; // MSCF

		if (in_DataSize >= minValidCABHeaderSize)
		{
			uint32_t sig;
			memcpy(&sig, in_spData.get(), sizeof(sig));

			if (sig == CABSignature)
			{
				LOG("Decompressing CAB-compressed AR...");
				ERF erf;

				const auto fdi = FDICreate(
					fdiAlloc,
					fdiFree,
					fdiOpen,
					fdiRead,
					fdiWrite,
					fdiClose,
					fdiSeek,
					cpuUNKNOWN,
					&erf
				);

				if (!fdi)
				{
					// TODO: Log error info from erf
					MessageBoxA(NULL, "FDICREATE FAILURE", "FDICREATE FAILURE", MB_OK);
				}

				archive_allocator arAllocator;
				rad::readonly_memory_stream compressedAR(in_spData.get(), in_DataSize);
				rad::memory_stream uncompressedAR(arAllocator);

				// OPTIMIZATION: We attempt one large allocation rather than many re-allocations.
				// If the allocation is not large enough, the stream will simply reallocate.
				uncompressedAR.reserve(in_DataSize * 2);

				// HACK: Taken from LibGens. Since the FDI interface doesn't allow us to pass
				// arbitrary user data to the fdiOpen callback, our source stream is converted
				// to a hex string and passed as the CAB path.
				char cabNameBuf[1] = {};
				char cabPathBuf[24] = {};
				std::sprintf(cabPathBuf, "%p", static_cast<rad::stream*>(&compressedAR));

				if (!FDICopy(
					fdi,
					cabNameBuf,
					cabPathBuf,
					0,
					fdiNotify,
					nullptr,
					static_cast<rad::stream*>(&uncompressedAR)))
				{
					// TODO: Log error info from erf
					MessageBoxA(NULL, "FDICOPY FAILURE", "FDICOPY FAILURE", MB_OK);
				}

				FDIDestroy(fdi);

				in_DataSize1 = in_DataSize = static_cast<uint32_t>(uncompressedAR.data().size());
				in_spData.reset(uncompressedAR.release().release());
			}
		}

		originalCArchiveDatabaseLoaderLoadArchive(
			This,
			in_spDatabase,
			in_spData,
			in_DataSize,
			in_DataSize1,
			in_pFileReader
		);
	}

	void InstallLegacyDataHandlers()
	{
		InitLegacyAudioPatcher();

		INSTALL_HOOK_ADDRESS(RegisterType, 0x140325750); // TODO: SIGSCAN
		INSTALL_HOOK_ADDRESS(CArchiveDatabaseLoaderLoadArchive, 0x140327400); // TODO: SIGSCAN

		INSTALL_HOOK_ADDRESS(criAtomExAcb_LoadAcbFile, g_cri->criAtomExAcb_LoadAcbFile);
		INSTALL_HOOK_ADDRESS(criatomplayer_set_wave_id_core, g_cri->criatomplayer_set_wave_id_core);
	}
}
