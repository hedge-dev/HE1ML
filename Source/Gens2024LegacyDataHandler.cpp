#include "Gens2024LegacyAudioGenerator.h"
#include "Gens2024LegacyAudioUpgrader.h"
#include "CRIWARE/Criware.h"
#include "Globals.h"
#include <boost/function.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/shared_ptr.hpp>
#include <rad/rad_memory_stream.h>
#include <rad/rad_file.h> // TODO
#include <rad/rad_path.h> // TODO
#include <string_view>

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
			//return boost::shared_ptr<CSoundCueSheetMemoryData>(
				//new CSoundCueSheetMemoryData()
			//);
		}

		//CSoundCueSheetMemoryData()
		//{
			// TODO
		//}
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

	static bool initializedCSBParser = false;

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
		LOG("Upgrading %s.csb ...", name.data);

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
			rad::memory_stream acbDataStream;
			if (!TryUpgradeCSB(acbDataStream, data, dataSize, name.data))
			{
				// TODO: Do we need to do anything else to handle the failure case?
				return;
			}

			// TODO: Use custom rad::allocator which calls this if this is in-fact necessary.
			// TODO: Handle case where an exception is thrown below and this memory could leak.
			void* newDataBuf = __HH_ALLOCALIGN(acbDataStream.data().size(), 0x10);
			std::memcpy(newDataBuf, acbDataStream.data().data(), acbDataStream.data().size());

			//const auto acbData = acbDataStream.data();
			const auto acbData = rad::span<unsigned char>(static_cast<unsigned char*>(newDataBuf), acbDataStream.data().size());
			const auto cueSheetBinaryData = static_cast<Hedgehog::Sound::CCueSheetBinaryData*>(dbData.get());

			cueSheetBinaryData->CreateCueSheet(
				name,
				acbData.data(),
				static_cast<unsigned long>(acbData.size())
			);

			dbData->SetMadeOne();
		}
	}

	HOOK(void, __cdecl, RegisterType, nullptr,
		Hedgehog::Database::CDatabase* thisPtr, Hedgehog::Base::CSharedString& name,
		TypeMakeFunc makeFunc, TypeCreateFunc createFunc)
	{
		if (!initializedCSBParser && std::strcmp(name.data, "acb") == 0)
		{
			Hedgehog::Base::CSharedString csbName("csb");

			originalRegisterType(
				thisPtr,
				csbName,
				boost::bind(MakeCSB, _1, _2, _3, _4),
				boost::bind(CreateCSBParser, _1, _2)
			);

			initializedCSBParser = true;
		}

		//Hedgehog::Mirage::CRenderingInfrastructure* renderInfra = nullptr;
		//Hedgehog::Mirage::CPhysicalAllocator* physAllocator = nullptr;

		//auto z = boost::bind(CreateCSBParser, _1, _2, _3, _4, renderInfra, false, physAllocator);
		//auto z = boost::bind(MakeCSB, _1, _2, _3, _4);
		//TypeMakeFunc aa = z;
		//sizeof(aa);

		LOG("Register type: %s", name.data);
		return originalRegisterType(thisPtr, name, makeFunc, createFunc);
	}

	HOOK(CriError, CRIAPI, criFsBinder_BindDirectory, nullptr,
		CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path,
		void* work, CriSint32 worksize, CriFsBindId* bndrid)
	{
		// TODO: REMOVE THIS HOOK
		LOG("BIND DIRECTORY: %s", path);
		return originalcriFsBinder_BindDirectory(bndrhn,
			srcbndrhn, path, work, worksize, bndrid);
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
		uint8_t idSize;
		uint8_t unknown1;
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

		inline const char* he1mlStrings() const noexcept
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

		const uint32_t* getHe1mlInfo(uint16_t id) const
		{
			const auto i = getWaveformIndex(id);
			if (i == waveformCount)
			{
				return nullptr;
			}

			const auto dataPos = dataPositions()[i];

			return reinterpret_cast<const uint32_t*>(
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

	static std::unordered_set<std::string> streaming_paths; // TODO: !!!

	HOOK(void, CRIAPI, criatomplayer_set_file_core, nullptr,
		CriAtomPlayerHn player, CriFsBinderHn binder, const char* path,
		size_t offset, size_t size)
	{
		LOG("AWB file: %s", path);
		originalcriatomplayer_set_file_core(player, binder, path, offset, size);
	}

	HOOK(void, CRIAPI, criatomplayer_set_wave_id_core, nullptr,
		CriAtomPlayerTag* player, CriAtomAwbTag* awb, CriSint32 id) //, CriUint32 offset)
	{
		// TODO: Possibly find a less hacky way to implement this

		if (awb->path)
		{
			if (streaming_paths.contains(awb->path))
			{
				//if (awb->memoryAwb && awb->memoryAwb->signature == 0x324C4D48)
				//if (awb->path && 
				//{
				LOG("FAKE AWB: %s", awb->path);
				//{
				const auto he1mlInfo = awb->streamingAwbToc->getHe1mlInfo(id);
				if (he1mlInfo)
				{
					const auto he1mlStrings = awb->streamingAwbToc->he1mlStrings();
					const auto redirectPath = he1mlStrings + he1mlInfo[0];

					LOG("override: \"%s\" -> \"%s\"", awb->path, redirectPath);

					g_cri->criatomplayer_set_file_core(player, nullptr, redirectPath, he1mlInfo[1], he1mlInfo[2]);

					player->offset = he1mlInfo[1];
					player->size = he1mlInfo[2];
					return;
				}
			}
			else
			{
				LOG("REAL AWB: %s", awb->path);
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
		//LOG("LOAD ACB FILE: %s", acb_path);
		//if (awb_path)
		//{
			//LOG("[USING AWB FILE: %s]", awb_path);
		//}


		// Mod redirection:

		// Gens 2011: /mods/foobar/disk/bb3/Sonic.arl
		// Gens 2011: /mods/foobar/Sound/SNG00_SYS.csb
		// Gens 2024: /mods/foobar/generations/raw/Sonic.arl
		// Gens 2024: /mods/foobar/generations/Sound/SNG00_SYS.acb

		// If a generations folder exists, also bind it, with higher
		// priority than the Gens 2011 subfolders.

		// This allows you to have mods which support both Gens 2011
		// and Gens 2024, and which have differences in Gens 2024.

		// This could be used, for example, to have custom stage mods
		// which support both games, but have additional chao objects
		// placed within the level if played in Gens 2024.



		// TODO: Replace all of this function with this logic:

		// 1: If an awb or acb is present, just redirect those files. [COMPLETE]
		// 2: Otherwise, if a csb file is present, create an acb from
		//    it ("upgrade" the csb to an acb). [MOSTLY DONE]
		// 3: If a cpk and/or aax streaming files are present, modify
		//    the (upgraded, or clean) acb using hardcoded aax path -> acb mappings. [TODO]



		std::string replaceAwbPath;
		StreamingDataType streamDataType = StreamingDataType::None;
		//bool useCpk = false, useCsb = false;
		EBindError r;

		std::string_view originalAwbPath;

		if (awb_path)
		{
			// If there are any mods to the awb file, use the original awb path.
			originalAwbPath = awb_path;

			r = g_loader->binder->ResolvePath(
				originalAwbPath.data(),
				&replaceAwbPath
			);

			// Otherwise, if there are not any mods to the awb file, check if
			// there are any Gens 2011 mods which modify the corresponding cpk.
			if (r == eBindError_NotFound)
			{
				assert(originalAwbPath.ends_with(".awb"));

				// HACK: Temporarily modify the const awb_path data.
				const auto awbExtPtr = const_cast<char*>(awb_path) + (originalAwbPath.size() - 4);

				std::memcpy(awbExtPtr, ".cpk", 4);

				r = g_loader->binder->ResolvePath(
					originalAwbPath.data(),
					&replaceAwbPath
				);

				std::memcpy(awbExtPtr, ".awb", 4);

				if (r == eBindError_None)
				{
					// Use the path to the Gens 2011 mod cpk as the awb path.
					awb_path = replaceAwbPath.c_str();
					streamDataType = StreamingDataType::Cpk;
				}
				else if (r == eBindError_NotFound)
				{
					std::memcpy(awbExtPtr, "/", 2);

					r = g_loader->binder->ResolvePath(
						originalAwbPath.data(),
						&replaceAwbPath
					);

					std::memcpy(awbExtPtr, ".awb", 4);

					awb_path = replaceAwbPath.c_str();
					streamDataType = StreamingDataType::Cpk_redirect_folder;
				}
			}
		}

		// If there are any mods to the acb file, use the original acb path.
		const std::string_view originalAcbPath(acb_path);
		std::string replaceAcbPath;

		r = g_loader->binder->ResolvePath(
			originalAcbPath.data(),
			&replaceAcbPath
		);

		if (r == eBindError_None)
		{
			if (streamDataType == StreamingDataType::Cpk)
			{
				LOG("Mod cpk + mod acb combination is not yet supported; falling back to mod acb redirect ONLY");
				awb_path = originalAwbPath.data();
			}
		}

		// Otherwise, if there are not any mods to the acb file, check if
		// there are any Gens 2011 mods which modify the corresponding csb.
		else if (r == eBindError_NotFound)
		{
			assert(originalAcbPath.ends_with(".acb"));

			// HACK: Temporarily modify the const acb_path data.
			const auto acbExtPtr = const_cast<char*>(acb_path) + (originalAcbPath.size() - 4);

			std::memcpy(acbExtPtr, ".csb", 4);

			r = g_loader->binder->ResolvePath(
				originalAcbPath.data(),
				&replaceAcbPath
			);

			std::memcpy(acbExtPtr, ".acb", 4);

			if (r == eBindError_None)
			{
				LOG("%s -> %s", originalAcbPath.data(), replaceAcbPath.c_str());

				const auto name = rad::path::get_stem(replaceAcbPath);
				LOG("Upgrading CriAu CueSheet... %s", name.data());

				rad::file_stream stream(
					replaceAcbPath.c_str(),
					rad::file_stream::OPEN_MODE_READ_ONLY |
					rad::file_stream::OPEN_FLAG_SHARED | // TODO: Should we get exclusive ownership of the csb file?
					rad::file_stream::OPEN_HINT_SEQUENTIAL_ACCESS // TODO: Should this be random access?
				);

				const auto csbDataSize = static_cast<unsigned long>(stream.get_size());
				std::unique_ptr<unsigned char[]> csbData(new unsigned char[csbDataSize]);

				stream.read(csbData.get(), csbDataSize);

				rad::memory_stream acbDataStream;
				if (TryUpgradeCSB(acbDataStream, csbData.get(), csbDataSize, name, streamDataType, awb_path))
				{
					//// TODO: Use custom rad::allocator which calls this if this is in-fact necessary.
					//void* newDataBuf = __HH_ALLOCALIGN(acbDataStream.data().size(), 0x10);
					//std::memcpy(newDataBuf, acbDataStream.data().data(), acbDataStream.data().size());

					streaming_paths.emplace(awb_path); // TODO: !!!

					const auto acbData = acb_data_array.emplace_back(std::move(acbDataStream)).data();

					//acb_path = replacePath.c_str();

					//UncheckedReplaceEndWith(originalAwbPath, "cpk"); // TODO: REMOVE THIS LINE

					const auto r2 = g_cri->criAtomExAcb_LoadAcbData(
						acbData.data(),
						static_cast<CriSint32>(acbData.size()),
						awb_binder,
						awb_path, //awb_path,
						nullptr, //work,
						0 //work_size
					);

					//const auto zz = *(void**)((char*)r2 + 0x10);
					//const auto z = (void*)((char*)zz + 0x1708);

					return r2;
				}
			}
			else if (r == eBindError_NotFound && streamDataType == StreamingDataType::Cpk)
			{
				LOG("Mod cpk + no acb/csb combination is not yet supported; falling back to no redirection");
				awb_path = originalAwbPath.data();
			}
		}

		return originalcriAtomExAcb_LoadAcbFile(acb_binder, acb_path, awb_binder, awb_path, work, work_size);
	}

	void InstallLegacyDataHandlers()
	{
		InitializeLegacyACBTemplates();

		//INSTALL_HOOK_ADDRESS(BindFile, values.cri_table.criFsBinder_BindFile);
		INSTALL_HOOK_ADDRESS(criAtomExAcb_LoadAcbFile, g_cri->criAtomExAcb_LoadAcbFile);
		//INSTALL_HOOK_ADDRESS(criAtomExAcb_LoadAcbData, values.cri_table.criAtomExAcb_LoadAcbData);

		//INSTALL_HOOK_ADDRESS(CreateCueSheetMemoryData, values.CreateCueSheetMemoryData);
		INSTALL_HOOK_ADDRESS(RegisterType, 0x140325750); // TODO
		//INSTALL_HOOK_ADDRESS(criFsiowin_Open, g_cri->criFsiowin_Open);

		INSTALL_HOOK_ADDRESS(criFsBinder_BindDirectory, g_cri->criFsBinder_BindDirectory);
		//g_cri->criFsBinder_BindCpk()

		INSTALL_HOOK_ADDRESS(criatomplayer_set_wave_id_core, g_cri->criatomplayer_set_wave_id_core);
		INSTALL_HOOK_ADDRESS(criatomplayer_set_file_core, g_cri->criatomplayer_set_file_core);

		//INSTALL_HOOK_ADDRESS(criatomsoudvoice_set_source, 0x1407bf730); // TODO
	}
}
