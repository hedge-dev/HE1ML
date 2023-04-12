#pragma once
#pragma push_macro("_ITERATOR_DEBUG_LEVEL")
#define _ITERATOR_DEBUG_LEVEL 2

#include <list>
#include "hhAllocator.h"

namespace Hedgehog
{
    template<typename T, typename TAllocator = Base::TAllocator<T>>
    class list : public std::list<T, TAllocator>
    {
    public:
        using std::list<T, TAllocator>::list;
    };

    // BB_ASSERT_SIZEOF(list<void*>, 0xC);
}

#pragma pop_macro("_ITERATOR_DEBUG_LEVEL")