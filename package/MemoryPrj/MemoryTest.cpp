#include "Memory.h"
#include "MemoryHelper.h"

#include <iostream>

int main()
{
    std::cout << "Trident restored Memory demo\n";
    std::cout << "Top module: Memory\n";

    bool ok = true;
    for (unsigned i = 0; i < 8; ++i) {
        const auto value = memoryPattern(i);
        ok = ok && verifyMemoryPattern(i, value);
        std::cout << "pattern[" << i << "] = 0x" << std::hex << value << std::dec << "\n";
    }

    std::cout << (ok ? "Memory helper test passed\n" : "Memory helper test failed\n");
    return ok ? 0 : 1;
}
