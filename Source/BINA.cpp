#include "BINA.h"

namespace bina
{
	const unsigned char* OffsetTableHandle::getRealOffTableEnd(
		const unsigned char* offTable, unsigned int offTableSize) noexcept
	{
		// Get the "real" end of offset table (not counting padding bytes).
		auto offTableEnd = (offTable + offTableSize);
		while (offTableEnd > offTable && *(offTableEnd - 1) == 0)
		{
			--offTableEnd;
		}

		return offTableEnd;
	}

	OffsetTableHandle::iterator& OffsetTableHandle::iterator::operator++()
	{
		// Increase the current offset table pointer as necessary.
		switch (*ptr & BINA_OFFSET_SIZE_MASK)
		{
		case BINA_OFFSET_SIZE_SIX_BIT:
			++ptr;
			break;

		case BINA_OFFSET_SIZE_FOURTEEN_BIT:
			ptr += 2;
			break;

		case BINA_OFFSET_SIZE_THIRTY_BIT:
			ptr += 4;
			break;

		default:
			// A size of 0 indicates that we've reached the end of the offset table.
			// NOTE: This won't happen with iterators returned by OffsetTableHandle,
			// as we already skip offset table padding in there.
			break;
		}

		return *this;
	}

	OffsetTableHandle::iterator& OffsetTableHandle::iterator::operator+=(difference_type v)
	{
		for (; v != 0; --v)
		{
			operator++();
		}

		return *this;
	}

	OffsetTableHandle::iterator operator+(
		OffsetTableHandle::iterator it,
		OffsetTableHandle::iterator::difference_type v)
	{
		return (it += v);
	}

	OffsetTableHandle::iterator operator+(
		OffsetTableHandle::iterator::difference_type v,
		OffsetTableHandle::iterator it)
	{
		return (it += v);
	}

	static unsigned int GrabSixBits(const unsigned char*& curOffTablePtr)
	{
		return static_cast<unsigned int>(*(curOffTablePtr++) & BINA_OFFSET_DATA_MASK);
	}

	static unsigned int GrabEightBits(const unsigned char*& curOffTablePtr)
	{
		return static_cast<unsigned int>(*(curOffTablePtr++));
	}

	static unsigned int GrabFourteenBits(const unsigned char*& curOffTablePtr)
	{
		unsigned int val = (GrabSixBits(curOffTablePtr) << 8);
		val |= GrabEightBits(curOffTablePtr);
		return val;
	}

	static unsigned int GrabThirtyBits(const unsigned char*& curOffTablePtr)
	{
		unsigned int val = (GrabSixBits(curOffTablePtr) << 24);
		val |= (GrabEightBits(curOffTablePtr) << 16);
		val |= (GrabEightBits(curOffTablePtr) << 8);
		val |= GrabEightBits(curOffTablePtr);
		return val;
	}

	OffsetTableHandle::iterator::value_type OffsetTableHandle::iterator::operator*() const
	{
		// Return the relative offset position at this position.
		auto curOffTablePtr = ptr;
		switch (*curOffTablePtr & BINA_OFFSET_SIZE_MASK)
		{
		case BINA_OFFSET_SIZE_SIX_BIT:
			return GrabSixBits(curOffTablePtr);

		case BINA_OFFSET_SIZE_FOURTEEN_BIT:
			return GrabFourteenBits(curOffTablePtr);

		case BINA_OFFSET_SIZE_THIRTY_BIT:
			return GrabThirtyBits(curOffTablePtr);

		default:
			// A size of 0 indicates that we've reached the end of the offset table.
			// NOTE: This won't happen with iterators returned by OffsetTableHandle,
			// as we already skip offset table padding in there.
			return 0;
		}
	}

	OffsetTableHandle::OffsetTableHandle(
		const unsigned char* offTable, unsigned int offTableSize) noexcept
		: offTableBeg(offTable)
		, offTableEnd(getRealOffTableEnd(offTable, offTableSize))
	{
	}

	void FixOffsets(OffsetTableHandle offTable, void* base)
	{
		auto curOffPtr = static_cast<unsigned int*>(base);
		const auto baseAddr = reinterpret_cast<unsigned int>(base);

		for (const auto relOffPos : offTable)
		{
			// Get pointer to current offset.
			curOffPtr += relOffPos;

			// Fix offset.
			*curOffPtr = (baseAddr + *curOffPtr);
		}
	}
}