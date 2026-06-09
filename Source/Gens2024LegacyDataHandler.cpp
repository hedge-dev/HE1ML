#include <string_view>
#include <exception> // TODO: Remove me
#include <boost/function.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/shared_ptr.hpp>
#include <rad/rad_memory_stream.h>
#include <rad/rad_file.h> // TODO
#include <rad/rad_path.h> // TODO
#include "Gens2024LegacyAudioUpgrader.h"
#include "Gens2024LegacyAudioPatcher.h"
#include "CRIWARE/Criware.h"
#include "Globals.h"

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

	HOOK(void, __cdecl, RegisterType, nullptr,
		Hedgehog::Database::CDatabase* thisPtr, Hedgehog::Base::CSharedString& name,
		TypeMakeFunc makeFunc, TypeCreateFunc createFunc)
	{
		if (std::strcmp(name.data, "acb") == 0)
		{
			// Also register csb type.
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

	void InstallLegacyDataHandlers()
	{
		InitLegacyAudioPatcher();

		INSTALL_HOOK_ADDRESS(criAtomExAcb_LoadAcbFile, g_cri->criAtomExAcb_LoadAcbFile);
		INSTALL_HOOK_ADDRESS(RegisterType, 0x140325750); // TODO
		INSTALL_HOOK_ADDRESS(criatomplayer_set_wave_id_core, g_cri->criatomplayer_set_wave_id_core);
	}
}
