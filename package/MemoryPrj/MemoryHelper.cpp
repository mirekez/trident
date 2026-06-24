#include "MemoryHelper.h"

std::uint32_t memoryPattern(unsigned index)
{
    return 0x1000u + index * 0x11u;
}

bool verifyMemoryPattern(unsigned index, std::uint32_t value)
{
    return value == memoryPattern(index);
}
