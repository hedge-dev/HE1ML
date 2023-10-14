#include "LWPackfile.h"
#include "BINA.h"

namespace lw::pacx
{
	const char* TypeDicNode::GetTypeSep() const
	{
		return std::strchr(name, ':');
	}

	bool TypeDicNode::IsOfType(const char* type) const
	{
		const auto typeSep = GetTypeSep();
		if (!typeSep) return false;

		return (std::strcmp(typeSep + 1, type) == 0);
	}

	bool TypeDicNode::IsSplitTable() const
	{
		return IsOfType("ResPacDepend");
	}

	void DataBlock::FixOffsets(void* base)
	{
		bina::OffsetTableHandle offTableHandle(Offsets(), offTableSize);
		bina::FixOffsets(offTableHandle, base);
	}

	void Header::Resolve()
	{
		if (blockCount)
		{
			// Fix all offsets in all blocks.
			auto curBlock = static_cast<DataBlock*>(FirstBlock());
			unsigned short i = 0;

			do
			{
				curBlock->FixOffsets(this);
			}
			while (++i < blockCount &&
				(curBlock = static_cast<DataBlock*>(curBlock->NextBlock()))
			);
		}

		// Mark header as resolved.
		// NOTE: The game checks this flag and doesn't resolve again if it's set, so this is safe.
		status |= PACX_HEADER_STATUS_RESOLVED;
	}

	void Header::ChainToNode(LinkedListNode* node)
	{
		// HACK: Replace the fileSize with a pointer to the given node, forming a linked list.
		// We also set a custom status value so that we can check if a pac has a next pointer or not.

		// NOTE: This is safe because the game never actually touches the fileSize.

		fileSize = reinterpret_cast<unsigned int>(node);
		status |= PACX_HEADER_STATUS_HE1ML_HAS_NEXT_NODE;
	}

	auto Builder::loadPac(const char* filePath) ->
		std::unique_ptr<LinkedListNode, PacBufferDeleter>
	{
		// Open packfile.
		std::unique_ptr<void, Win32FileDeleter> hFile(
			CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY,
				nullptr)
		);

		if (hFile.get() == INVALID_HANDLE_VALUE)
		{
			return nullptr;
		}

		const auto size = GetFileSize(hFile.get(), nullptr);

		// Setup linked list node.
		std::unique_ptr<LinkedListNode, PacBufferDeleter> pacBuffer(
			static_cast<LinkedListNode*>(allocator->Alloc(sizeof(LinkedListNode) + size, 1))
		);

		pacBuffer->next = nullptr;
		pacBuffer->allocator = allocator;

		// Read packfile.
		ReadFile(hFile.get(), pacBuffer->data(), size, nullptr, nullptr);
		hFile.reset(); // NOTE: This closes the file.

		// Fix the offsets in the packfile.
		pacBuffer->data()->Resolve();

		return pacBuffer;
	}

	void Builder::addPacToList(LinkedListNode* node)
	{
		// Chain the previous pac to this pac.
		if (prevNode)
		{
			prevNode->next = node;
		}

		prevNode = node;

		// Add append packfile to our list of packfiles to keep it alive.
		pacBuffers.push_back(node);
	}

	void Builder::freePacBuffers()
	{
		for (const auto pacBuffer : pacBuffers)
		{
			allocator->Free(pacBuffer);
		}

		pacBuffers.clear();
	}

	void Builder::Start(void* pacData, csl::fnd::IAllocator* allocator)
	{
		// Fix root packfile offsets.
		const auto pac = static_cast<Header*>(pacData);
		pac->Resolve();

		// Setup builder.
		this->allocator = allocator;
		origPac = pac;
	}

	void Builder::MergeWithAppend(const std::string& appendPacFilePath)
	{
		// Load append packfile.
		auto aPacBuffer = loadPac(appendPacFilePath.c_str());

		// Replace all corresponding data pointers in original pac with data pointers from append pac.
		// NOTE: a = append, o = original.
		// TODO: Repeat for each data block if there are multiple data blocks.
		const auto aPac = aPacBuffer.get()->data();
		const auto aDataBlock = static_cast<pacx::DataBlock*>(aPac->FirstBlock());
		const auto& aTypes = aDataBlock->Types();

		const auto oDataBlock = static_cast<pacx::DataBlock*>(origPac->FirstBlock());
		const auto& oTypes = oDataBlock->Types();

		unsigned int aPacTotalSplitCount = 0;
		bool aPacHasProxyEntries = aDataBlock->HasProxies();

		// NOTE: This is a field of the Builder rather than a local variable for this
		// function only to prevent the vector from having to be re-allocated;
		// you know what they say, gotta go fast!
		aPacCurSplitTableFiles.clear();

		for (const auto& aTypeNode : aTypes)
		{
			const auto& aFiles = *aTypeNode.data;

			// Skip split tables.
			if (aTypeNode.IsSplitTable())
			{
				aPacCurSplitTableFiles.push_back(&aFiles);

				// Increase total split count.
				for (const auto& aFileNode : aFiles)
				{
					aPacTotalSplitCount += aFileNode.data->Data<SplitTable>()->count;
				}

				continue;
			}

			// Handle all other types.
			const auto oTypeNode = oTypes.find(aTypeNode.name);

			if (oTypeNode != oTypes.end())
			{
				const auto& oFiles = *oTypeNode->data;

				for (const auto& aFileNode : aFiles)
				{
					const auto oFileNode = oFiles.find(aFileNode.name);

					if (oFileNode != oFiles.end())
					{
						// Replace data pointer in original pac with data pointer in append pac.
						oFileNode->data = aFileNode.data;
						++replacedFileCount;
					}
					else
					{
						++newFileCount;
					}
				}
			}
			else
			{
				++newTypeCount;
			}
		}

		// Exit early and discard the append pac if there are no new or replaced files (the append pac has no files).
		if (newFileCount == 0 && replacedFileCount == 0)
		{
			return;
		}

		// Add append packfile to our builder's list of packfiles so that it doesn't get freed until later.
		addPacToList(aPacBuffer.get());
		aPacBuffer.release(); // NOTE: This will be freed by the builder's destructor.

		// Import append packfile splits if necessary.
		if (!aPacCurSplitTableFiles.empty() && aPacHasProxyEntries)
		{
			// Copy append pac file path into splitPathBuf.
			char splitPathBuf[0x400];
			const auto appendPacFilePathLen = appendPacFilePath.size();
			std::size_t splitNamePos = 0;

			std::memcpy(splitPathBuf, appendPacFilePath.c_str(), appendPacFilePathLen + 1);

			// Go backwards from the end of the append pac file path
			// until we find the position of the file name.
			for (std::size_t i = appendPacFilePathLen; i != 0; --i)
			{
				if (splitPathBuf[i] == '\\' || splitPathBuf[i] == '/')
				{
					splitNamePos = (i + 1);
					break;
				}
			}

			// Set append pac remaining split count to the number of splits we have to load.
			// NOTE: This is necessary, as the game's Packfile::Import function reads this value.
			aPac->remainingSplitCount = static_cast<unsigned char>(aPacTotalSplitCount);

			// Load and import the append pac's splits.
			for (const auto aFiles : aPacCurSplitTableFiles)
			{
				for (const auto& aFileNode : *aFiles)
				{
					const auto aSplitTable = aFileNode.data->Data<SplitTable>();

					for (const auto& aSplitInfo : *aSplitTable)
					{
						// Copy the split name into splitPathBuf.
						std::strcpy(splitPathBuf + splitNamePos, aSplitInfo.name);

						// Load the append pac split.
						auto aSplitPacBuffer = loadPac(splitPathBuf);
						const auto aSplitPac = aSplitPacBuffer.get()->data();

						// Add split packfile to our builder's list of packfiles so that it doesn't get freed until later.
						addPacToList(aSplitPacBuffer.get());
						aSplitPacBuffer.release(); // NOTE: This will be freed by the builder's destructor.

						// Import the split's data into the append pac.
						// TODO: Repeat this for every data block.
						const auto aSplitDataBlock = static_cast<pacx::DataBlock*>(aSplitPac->FirstBlock());
						const auto& aSplitTypes = aSplitDataBlock->Types();
						const auto aProxyTable = aDataBlock->Proxies();

						for (const auto& aProxyEntry : *aProxyTable)
						{
							const auto oTypeNode = oTypes.find(aProxyEntry.type);
							const auto aSplitTypeNode = aSplitTypes.find(aProxyEntry.type);

							if (oTypeNode != oTypes.end() && aSplitTypeNode != aSplitTypes.end())
							{
								const auto& oFiles = *oTypeNode->data;
								const auto oFileNode = oFiles.find(aProxyEntry.name);
								const auto& aSplitFiles = *aSplitTypeNode->data;
								const auto aSplitFileNode = aSplitFiles.find(aProxyEntry.name);

								if (oFileNode != oFiles.end() && aSplitFileNode != aSplitFiles.end())
								{
									// Replace data pointer in original pac with data pointer in split pac.
									oFileNode->data = aSplitFileNode->data;
								}
							}
						}
					}
				}
			}
		}
	}

	Header* Builder::Finish(std::size_t& finalPacSize)
	{
		// Build a final packfile which we can pass to the game.
		Header* finalPac;

		if (newFileCount == 0)
		{
			// If there are no new files, we don't need to build a new packfile!
			// Just return the original packfile, which we've already modified.
			finalPac = origPac;
			finalPacSize = origPac->fileSize;

			if (!pacBuffers.empty())
			{
				origPac->ChainToNode(pacBuffers[0]);
			}
		}
		else
		{
			// TODO: Build new packfile that points to origPac data.
			finalPac = nullptr;
			__debugbreak();
		}

		// Reset for future iterations.

		// NOTE: We purposefully call this instead of freePacBuffers()
		// because we do *not* want to actually call Free on each pac
		// buffer as we are about to give these pacs to the game!
		// The pacs will be cleaned up later in our hook of Packfile::Cleanup()
		pacBuffers.clear();

		allocator = nullptr;
		origPac = nullptr;
		prevNode = nullptr;
		newTypeCount = 0;
		replacedFileCount = 0;
		newFileCount = 0;

		return finalPac;
	}

	Builder::Builder()
	{
		// NOTE: There's never actually more than 1 split table in real files.
		aPacCurSplitTableFiles.reserve(1);

		pacBuffers.reserve(100);
	}

	Builder::~Builder()
	{
		freePacBuffers();
	}
}