#pragma once

#include "hhAllocator.h"

namespace Hedgehog::Base
{
	template<typename T>
	class CCowData
	{
	protected:
		class CData
		{
		public:
			size_t m_RefCountAndLength;
			T m_Data[1];
		};

		static constexpr const T* ms_memStatic = (const T*)0x13E0DC0;

		const T* m_ptr;

		bool IsMemStatic() const
		{
			return !m_ptr || m_ptr == ms_memStatic;
		}

		CData* GetData() const
		{
			return (CData*)((char*)m_ptr - offsetof(CData, m_Data));
		}

		void SetData(CData* pData)
		{
			m_ptr = (const T*)((char*)pData + offsetof(CData, m_Data));
		}

	public:
		const T* Get() const
		{
			return IsMemStatic() ? ms_memStatic : m_ptr;
		}

		void Unset()
		{
			if (!IsMemStatic() && (uint16_t)InterlockedDecrement(&GetData()->m_RefCountAndLength) == 0)
				__HH_FREE(GetData());

			m_ptr = ms_memStatic;
		}

		void Set(const CCowData& other)
		{
			m_ptr = other.m_ptr;

			if (!IsMemStatic())
				InterlockedIncrement(&GetData()->m_RefCountAndLength);
		}

		void Set(const T* pPtr, const size_t length, const void* pSuffix, const size_t suffix_length)
		{
			Unset();

			if (!length)
				return;

			size_t memSize = (offsetof(CData, m_Data) + suffix_length + length * sizeof(T));
			const size_t memSizeAligned = (memSize + 0x10) & 0xFFFFFFF0;
			memSize -= suffix_length;

			CData* pData = (CData*)__HH_ALLOC(memSizeAligned);
			pData->m_RefCountAndLength = (length << 16) | 1;

			if (pPtr)
			{
				memcpy(pData->m_Data, pPtr, length * sizeof(T));
				memset(&pData->m_Data[length], 0, memSizeAligned - memSize);

				if (pSuffix)
				{
					memcpy((char*)pData + memSize, pSuffix, suffix_length);
				}
			}

			SetData(pData);
		}

		void Set(const T* pPtr, const size_t length)
		{
			Set(pPtr, length, nullptr, 0);
		}

		size_t Size() const
		{
			return IsMemStatic() ? 0 : (GetData()->m_RefCountAndLength >> 16);
		}

		CCowData() : m_ptr(ms_memStatic)
		{

		}

		CCowData(CCowData&& other)
		{
			m_ptr = other.m_ptr;
			other.m_ptr = ms_memStatic;
		}

		CCowData(const CCowData& other)
		{
			Set(other);
		}

		~CCowData()
		{
			Unset();
		}
	};
}