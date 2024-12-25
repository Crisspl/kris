#include "resource_allocator.h"

uint64_t kris::ResourceAllocator::Allocation::static_getNewId()
{
    static uint64_t id_gen = 0ULL; // atomic?

    return id_gen++;
}
