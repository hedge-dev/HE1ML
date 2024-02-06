#pragma once

namespace bina
{
	enum OffsetFlags : unsigned char
	{
		/* Masks */
		BINA_OFFSET_SIZE_MASK = 0xC0U,
		BINA_OFFSET_DATA_MASK = 0x3FU,

		/* Sizes */
		BINA_OFFSET_SIZE_SIX_BIT = 0x40U,
		BINA_OFFSET_SIZE_FOURTEEN_BIT = 0x80U,
		BINA_OFFSET_SIZE_THIRTY_BIT = 0xC0U,
	};

	class OffsetTableHandle
	{
	protected:
		const unsigned char* offTableBeg;
		const unsigned char* offTableEnd;

		static const unsigned char* getRealOffTableEnd(
			const unsigned char* offTable,
			unsigned int offTableSize) noexcept;

	public:
		class iterator
		{
			const unsigned char* ptr;

		public:
			using value_type = unsigned int;
			using difference_type = std::ptrdiff_t;
			using pointer = value_type*;
			using reference = value_type&;
			using iterator_category = std::forward_iterator_tag;

			iterator& operator++();

			inline iterator operator++(int)
			{
				iterator tmpCopy(*this);
				operator++();
				return tmpCopy;
			}

			iterator& operator+=(difference_type v);

			friend iterator operator+(iterator it, difference_type v);

			friend iterator operator+(difference_type v, iterator it);

			value_type operator*() const;

			inline bool operator==(iterator other) const noexcept
			{
				return (ptr == other.ptr);
			}

			inline bool operator!=(iterator other) const noexcept
			{
				return (ptr != other.ptr);
			}

			inline bool operator<(iterator other) const noexcept
			{
				return (ptr < other.ptr);
			}

			inline bool operator>(iterator other) const noexcept
			{
				return (ptr > other.ptr);
			}

			inline bool operator<=(iterator other) const noexcept
			{
				return (ptr <= other.ptr);
			}

			inline bool operator>=(iterator other) const noexcept
			{
				return (ptr >= other.ptr);
			}

			inline iterator() noexcept = default;

			inline iterator(const unsigned char* ptr) noexcept
				: ptr(ptr) {}
		};

		inline iterator begin() const noexcept
		{
			return iterator(offTableBeg);
		}

		inline iterator end() const noexcept
		{
			return iterator(offTableEnd);
		}

		OffsetTableHandle(const unsigned char* offTable, unsigned int offTableSize) noexcept;
	};

	void FixOffsets(OffsetTableHandle offTable, void* base);
}