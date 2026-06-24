#pragma once

#include "cpphdl.h"

using namespace cpphdl;

extern long _system_clock;

#if defined(VERILATOR)
#define TRIDENT_STRINGIFY_IMPL(name) #name
#define TRIDENT_STRINGIFY(name) TRIDENT_STRINGIFY_IMPL(name)
#define TRIDENT_VERILATOR_HEADER(name) TRIDENT_STRINGIFY(name.h)
#include TRIDENT_VERILATOR_HEADER(VERILATOR_MODEL)
#endif

class MemoryTest : public Module
{
public:
    _PORT(logic<1>) valid_in;
    _PORT(logic<1>) ready_out = _ASSIGN_COMB(ready_out_value);

private:
    logic<1> ready_out_value{};
    reg<u<8>> state_reg;
    _LAZY_COMB(ready_comb, logic<1>)
        ready_comb = {};
        return ready_comb;
    }

public:
    void _work(bool reset)
    {
        (void)reset;
        state_reg._next = state_reg;
    }

    void _strobe()
    {
        state_reg.strobe();
    }

    void _assign() {}
};

#if !defined(SYNTHESIS)
class MemoryTestTestHarness : public Module
{
public:
#if defined(VERILATOR)
    VERILATOR_MODEL dut;
#else
    MemoryTest dut;
#endif

    void _work(bool reset)
    {
#if defined(VERILATOR)
        dut.clk = 0;
        dut.reset = reset ? 1 : 0;
        dut.eval();
#else
        dut._assign();
        dut._work(reset);
#endif
    }

    void _strobe()
    {
#if !defined(VERILATOR)
        dut._strobe();
#endif
    }

    void _assign() {}
};

#if defined(TRIDENT_CLASS_GENERATOR_STANDALONE)
long _system_clock = -1;
int main()
{
    MemoryTestTestHarness harness;
    harness._assign();
    harness._work(true);
    harness._strobe();
    ++_system_clock;
    harness._work(false);
    harness._strobe();
    return 0;
}
#endif
#endif
