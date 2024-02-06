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

	std::string_view& Builder::addOrGetNewTypeName(std::string_view newTypeName)
	{
		for (auto& existingNewTypeName : newTypeNames)
		{
			if (existingNewTypeName == newTypeName)
			{
				return existingNewTypeName;
			}
		}

		return newTypeNames.emplace_back(std::move(newTypeName));
	}

	const Builder::NewFileInfo* Builder::getNextFileInfoWithNamePtr(
		const NewFileInfo* begin, const NewFileInfo* end,
		const char* typeNamePtr) const
	{
		for (; begin != end; ++begin)
		{
			// NOTE: We check the actual pointer itself instead of doing a string comparison!
			// This is much faster, but we have to be careful!
			// This is only safe because of the very specific way in which we always
			// add the pointers and check their values, ensuring that they will always
			// either match, or represent two different strings.
			if (begin->typeName == typeNamePtr)
			{
				return begin;
			}
		}

		return nullptr;
	}

	auto Builder::allocPacBuffer(std::size_t pacSize) const ->
		std::unique_ptr<LinkedListNode, PacBufferDeleter>
	{
		std::unique_ptr<LinkedListNode, PacBufferDeleter> pacBuffer(
			static_cast<LinkedListNode*>(allocator->Alloc(sizeof(LinkedListNode) + pacSize, 16))
		);

		pacBuffer->next = nullptr;
		pacBuffer->allocator = allocator;

		return pacBuffer;
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

		// Read packfile.
		auto pacBuffer = allocPacBuffer(size);
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
			pacBuffer->allocator->Free(pacBuffer);
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
		if (!aPacBuffer) return;

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
						// This is a new file not present in the original pac; store info on it for later.
						newFiles.emplace_back(oTypeNode->name, &aFileNode);
					}
				}
			}
			else if (aFiles.count > 0) // NOTE: We don't count empty types as new types.
			{
				// This is a new type not present in the original pac; store its name for later.
				const auto newTypeName = addOrGetNewTypeName(aTypeNode.name).data();

				// Store info on the new file names for later.
				for (const auto& aFileNode : aFiles)
				{
					newFiles.emplace_back(newTypeName, &aFileNode);
				}
			}
		}

		// Exit early and discard the append pac if there are no new or replaced files (the append pac has no files).
		if (newFiles.empty() && replacedFileCount == 0)
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
						if (!aSplitPacBuffer) continue;

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
							// Find the type in the append split pac.
							const auto oTypeNode = oTypes.find(aProxyEntry.type);
							const auto aSplitTypeNode = aSplitTypes.find(aProxyEntry.type);

							if (aSplitTypeNode == aSplitTypes.end())
							{
								continue;
							}

							// Find the file in the append split pac.
							const auto& aSplitFiles = *aSplitTypeNode->data;
							const auto aSplitFileNode = aSplitFiles.find(aProxyEntry.name);

							if (aSplitFileNode == aSplitFiles.end())
							{
								continue;
							}

							// Replace the file data pointer in the original pac with the
							// data pointer in the append split pac, unless the file isn't
							// present in the original pac (it is a new file).
							if (oTypeNode != oTypes.end())
							{
								const auto& oFiles = *oTypeNode->data;
								const auto oFileNode = oFiles.find(aProxyEntry.name);

								if (oFileNode != oFiles.end())
								{
									oFileNode->data = aSplitFileNode->data;
									continue; // Skip the following code.
								}
							}

							// If the file wasn't present in the original pac, replace the
							// file data pointer in the append pac instead.
							const auto aTypeNode = aTypes.find(aProxyEntry.type);

							if (aTypeNode != aTypes.end())
							{
								const auto& aFiles = *aTypeNode->data;
								const auto aFileNode = aFiles.find(aProxyEntry.name);

								if (aFileNode != aFiles.end())
								{
									aFileNode->data = aSplitFileNode->data;
								}
							}
						}
					}
				}
			}
		}
	}

	void Builder::Finish()
	{
		// Build extra data and assign it to the original packfile, if necessary.
		const auto finalPac = origPac;

		if (!newFiles.empty())
		{
			// Calculate total type and file counts.
			// TODO: Repeat for each data block in the file.
			auto oDataBlock = static_cast<DataBlock*>(origPac->FirstBlock());
			auto& oTypes = oDataBlock->Types();

			const auto totalTypeCount = (oTypes.count + newTypeNames.size());
			auto totalFileCount = newFiles.size();

			for (const auto& oTypeNode : oTypes)
			{
				const auto& oFiles = *oTypeNode.data;
				totalFileCount += oFiles.count;
			}

			// Calculate extra pac data size.
			// NOTE: This may be larger than the size that is actually required, due
			// to us not accounting for duplicate type/file names, but it's fine;
			// the amount of memory potentially wasted is not very large.
			const auto exPacDataSize = (
				((sizeof(TypeDicNode) + sizeof(FileDic)) * totalTypeCount) +
				(sizeof(FileDicNode) * totalFileCount)
			);

			auto exPacBuffer = allocPacBuffer(exPacDataSize);

			if (!pacBuffers.empty())
			{
				exPacBuffer->next = pacBuffers[0];
			}

			// Chain origPac to exPacBuffer
			origPac->ChainToNode(exPacBuffer.get());

			// Copy type nodes from original pac into extra pac data.
			// NOTE: We are going to remap the pointers later as necessary.
			const auto exTypeNodes = exPacBuffer->data<TypeDicNode>();

			std::memcpy(exTypeNodes, oTypes.data, sizeof(TypeDicNode) * oTypes.count);

			// Remap original pac type nodes pointer to point to extra pac data.
			oTypes.data = exTypeNodes;
			auto& exTypes = oTypes; // NOTE: This is just to avoid confusion when reading this code.

			// Setup new type dictionaries.
			for (const auto& newTypeName : newTypeNames)
			{
				auto& exTypeNode = exTypes[exTypes.count++];

				exTypeNode.name = newTypeName.data();
				exTypeNode.data = nullptr; // NOTE: We'll fill this in later.
			}

			// Setup new file dictionaries/file nodes.
			auto exFiles = reinterpret_cast<FileDic*>(exTypes.end());
			const auto newFilesEnd = (newFiles.data() + newFiles.size());

			for (auto& exTypeNode : exTypes)
			{
				// If there are no new files of this type, just re-use the original file dictionary.
				auto newFileInfo = getNextFileInfoWithNamePtr(
					newFiles.data(), newFilesEnd, exTypeNode.name);

				if (!newFileInfo)
				{
					continue;
				}

				// Otherwise, setup a new file dictionary.
				const auto exFileNodes = reinterpret_cast<FileDicNode*>(exFiles + 1);

				exFiles->count = 0;
				exFiles->data = exFileNodes;

				// Copy original file nodes into the new file dictionary, if any.
				if (exTypeNode.data)
				{
					const auto& oFiles = *exTypeNode.data;

					std::memcpy(exFileNodes, oFiles.data, sizeof(FileDicNode) * oFiles.count);
					exFiles->count += oFiles.count;
				}

				exTypeNode.data = exFiles;

				// Copy new file nodes into the new file dictionary.
				do
				{
					auto exFileNode = exFiles->find(newFileInfo->dicNode->name);

					if (exFileNode == exFiles->end())
					{
						++exFiles->count;
					}

					*exFileNode = *newFileInfo->dicNode;

					newFileInfo = getNextFileInfoWithNamePtr(
						newFileInfo + 1, newFilesEnd, exTypeNode.name);
				}
				while (newFileInfo);

				// Increase exFiles pointer for next iteration.
				exFiles = reinterpret_cast<FileDic*>(exFiles->end());
			}

			// Release exPacBuffer memory.
			// NOTE: We do not have to add it to the pac list for it to
			// be freed, since origPac has already been chained to it.
			exPacBuffer.release();
		}

		// If there are no new files, we don't need to build any extra data!
		// Just use the original packfile, which we've already modified.
		else
		{
			if (!pacBuffers.empty())
			{
				origPac->ChainToNode(pacBuffers[0]);
			}
		}

		// Reset for future iterations.
		newTypeNames.clear();
		newFiles.clear();

		// NOTE: We purposefully call this instead of freePacBuffers()
		// because we do *not* want to actually call Free on each pac
		// buffer as we are about to give these pacs to the game!
		// The pacs will be cleaned up later in our hook of Packfile::Cleanup()
		pacBuffers.clear();

		allocator = nullptr;
		origPac = nullptr;
		prevNode = nullptr;
		replacedFileCount = 0;
	}

	Builder::Builder()
	{
		newTypeNames.reserve(30);
		newFiles.reserve(1000);
		// NOTE: There's never actually more than 1 split table in real files.
		aPacCurSplitTableFiles.reserve(1);
		pacBuffers.reserve(100);
	}

	Builder::~Builder()
	{
		freePacBuffers();
	}
}