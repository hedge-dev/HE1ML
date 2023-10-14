#pragma once
#include <Sonic2013/Sonic2013.h>

namespace lw::pacx
{
	struct LinkedListNode;

	template<typename T>
	struct Table
	{
		unsigned int count;
		T* data;

		inline unsigned int size() const noexcept
		{
			return count;
		}

		inline T* begin() const noexcept
		{
			return data;
		}

		inline T* end() const noexcept
		{
			return data + count;
		}

		inline T& operator[](unsigned int pos) const noexcept
		{
			return data[pos];
		}
	};

	struct ProxyEntry
	{
		char* type;
		char* name;
		unsigned int nodeIndex;
	};

	using ProxyTable = Table<ProxyEntry>;

	struct DataEntry
	{
		unsigned int dataSize;
		void* customDataPtr;
		unsigned int unknown1;
		unsigned char flags;
		unsigned char status;
		unsigned short unknown2;

		inline bool IsProxy() const noexcept
		{
			return (flags & 0x80);
		}

		template<typename T = void>
		inline const T* Data() const noexcept
		{
			return reinterpret_cast<const T*>(this + 1);
		}

		template<typename T = void>
		inline T* Data() noexcept
		{
			return reinterpret_cast<T*>(this + 1);
		}
	};

	struct SplitInfo
	{
		char* name;
	};

	struct SplitTable
	{
		SplitInfo* data;
		unsigned int count;

		inline SplitInfo* begin() const noexcept
		{
			return data;
		}

		inline SplitInfo* end() const noexcept
		{
			return data + count;
		}

		inline SplitInfo& operator[](unsigned int pos) const noexcept
		{
			return data[pos];
		}
	};

	template<typename T>
	struct DicNode
	{
		using value_type = T;

		char* name;
		T* data;

		bool operator==(const char* name) const
		{
			return (std::strcmp(this->name, name) == 0);
		}
	};

	template<typename NodeType>
	struct LinearDic : public Table<NodeType>
	{
		NodeType* find(const char* key) const
		{
			auto it = this->begin();
			for (; it != this->end(); ++it)
			{
				if (*it == key)
				{
					return it;
				}
			}

			return it;
		}

		inline NodeType& operator[](const char* key) const
		{
			return *find(key);
		}

		using Table<NodeType>::operator[];
	};

	using FileDicNode = DicNode<DataEntry>;

	using FileDic = LinearDic<FileDicNode>;

	struct TypeDicNode : public DicNode<FileDic>
	{
		const char* GetTypeSep() const;

		bool IsOfType(const char* type) const;

		bool IsSplitTable() const;
	};

	struct TypeDic : public LinearDic<TypeDicNode>
	{
		TypeDicNode* find_type(const char* typeName) const
		{
			auto it = this->begin();
			for (; it != this->end(); ++it)
			{
				if (it->IsOfType(typeName))
				{
					return it;
				}
			}

			return it;
		}
	};

	struct DataBlock
	{
		unsigned int signature;
		unsigned int size;
		unsigned int dataEntriesSize;
		unsigned int dicsSize;
		unsigned int proxiesSize;
		unsigned int strTableSize;
		unsigned int offTableSize;
		unsigned char dicDepth;
		unsigned char type;
		unsigned short padding;

		inline const void* NextBlock() const noexcept
		{
			return (reinterpret_cast<const unsigned char*>(this) + size);
		}

		inline void* NextBlock() noexcept
		{
			return (reinterpret_cast<unsigned char*>(this) + size);
		}

		inline bool HasProxies() const noexcept
		{
			return (proxiesSize != 0);
		}

		inline const TypeDic& Types() const noexcept
		{
			return *reinterpret_cast<const TypeDic*>(this + 1);
		}

		inline TypeDic& Types() noexcept
		{
			return *reinterpret_cast<TypeDic*>(this + 1);
		}

		inline const void* DataEntriesSection() const noexcept
		{
			return (reinterpret_cast<const unsigned char*>(&Types()) + dicsSize);
		}

		inline void* DataEntriesSection() noexcept
		{
			return (reinterpret_cast<unsigned char*>(&Types()) + dicsSize);
		}

		inline const ProxyTable* Proxies() const noexcept
		{
			return reinterpret_cast<const ProxyTable*>(
				static_cast<const unsigned char*>(DataEntriesSection()) +
				dataEntriesSize);
		}

		inline ProxyTable* Proxies() noexcept
		{
			return reinterpret_cast<ProxyTable*>(
				static_cast<unsigned char*>(DataEntriesSection()) +
				dataEntriesSize);
		}

		inline const char* Strings() const noexcept
		{
			return (reinterpret_cast<const char*>(Proxies()) + proxiesSize);
		}

		inline char* Strings() noexcept
		{
			return (reinterpret_cast<char*>(Proxies()) + proxiesSize);
		}

		inline const unsigned char* Offsets() const noexcept
		{
			return (reinterpret_cast<const unsigned char*>(Strings()) + strTableSize);
		}

		inline unsigned char* Offsets() noexcept
		{
			return (reinterpret_cast<unsigned char*>(Strings()) + strTableSize);
		}

		void FixOffsets(void* base);
	};

	enum HeaderStatus : unsigned char
	{
		PACX_HEADER_STATUS_NONE = 0,
		PACX_HEADER_STATUS_RESOLVED = 16,
		PACX_HEADER_STATUS_HE1ML_HAS_NEXT_NODE = 128,
	};

	struct Header
	{
		char signature[4];
		char version[3];
		char endianFlag;
		unsigned int fileSize;
		unsigned short blockCount;
		unsigned char status;
		unsigned char remainingSplitCount;

		inline const void* FirstBlock() const noexcept
		{
			return (this + 1);
		}

		inline void* FirstBlock() noexcept
		{
			return (this + 1);
		}

		void Resolve();

		void ChainToNode(LinkedListNode* node);
	};

	struct alignas(16) LinkedListNode
	{
		LinkedListNode* next;
		csl::fnd::IAllocator* allocator;

		inline const Header* data() const noexcept
		{
			return reinterpret_cast<const Header*>(this + 1);
		}

		inline Header* data() noexcept
		{
			return reinterpret_cast<Header*>(this + 1);
		}
	};

	class Builder
	{
		struct Win32FileDeleter
		{
			void operator()(HANDLE hFile)
			{
				CloseHandle(hFile);
			}
		};

		struct PacBufferDeleter
		{
			void operator()(LinkedListNode* node)
			{
				node->allocator->Free(node);
			}
		};

		std::vector<const FileDic*> aPacCurSplitTableFiles;
		std::vector<LinkedListNode*> pacBuffers;
		csl::fnd::IAllocator* allocator = nullptr;
		Header* origPac = nullptr;
		LinkedListNode* prevNode = nullptr;
		std::size_t newTypeCount = 0;
		std::size_t replacedFileCount = 0;
		std::size_t newFileCount = 0;

		std::unique_ptr<LinkedListNode, PacBufferDeleter> loadPac(const char* filePath);

		void addPacToList(LinkedListNode* data);

		void freePacBuffers();

	public:
		void Start(void* pacData, csl::fnd::IAllocator* allocator);

		void MergeWithAppend(const std::string& appendPacFilePath);

		Header* Finish(std::size_t& finalPacSize);

		Builder();

		~Builder();
	};
}