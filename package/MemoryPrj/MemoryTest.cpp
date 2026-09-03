#include "MemoryTest.h"

#include <iostream>

int main()
{
    std::cout << "Trident Memory demo\n";
    std::cout << "Top module: Memory\n";

    MemoryTestHarness harness;
    harness.__inst_name = "MemoryTestHarness";
    harness._assign();

    harness._work(true);
    harness._strobe();
    ++_system_clock;

    for (int cycle = 0; cycle < 8 && !harness.test.done_out(); ++cycle) {
        harness._work(false);
        harness._strobe();
        ++_system_clock;
    }

    const bool ok = harness.passed();
    std::cout << (ok ? "Memory module test passed\n" : "Memory module test failed\n");
    return ok ? 0 : 1;
}
