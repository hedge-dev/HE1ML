#pragma once

#include <list>
#include "hhAllocator.h"

namespace Hedgehog
{
    template<typename T, typename TAllocator = Base::TAllocator<T>>
    class list :

#if _ITERATOR_DEBUG_LEVEL == 0
        insert_padding<4>,
#endif
        public std::list<T, TAllocator>

    {
    public:
        using std::list<T, TAllocator>::list;
    };

    ASSERT_SIZEOF(list<void*>, 0xC);
}