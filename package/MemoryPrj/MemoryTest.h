#pragma once

#include "Memory.h"
#include "cpphdl.h"

#include <cstring>
#include <cstdint>

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
    _PORT(u<clog2(Memory::MEM_DEPTH)>) addr_out = _ASSIGN_REG(addr_reg);
    _PORT(bool) write_out = _ASSIGN_COMB(write_bool_func());
    _PORT(logic<Memory::MEM_WIDTH_BYTES * 8>) write_data_out = _ASSIGN_REG(write_data_reg);
    _PORT(logic<Memory::MEM_WIDTH_BYTES>) write_mask_out = _ASSIGN_REG(write_mask_reg);
    _PORT(bool) read_out = _ASSIGN_COMB(read_bool_func());
    _PORT(logic<Memory::MEM_WIDTH_BYTES * 8>) read_data_in;
    _PORT(bool) done_out = _ASSIGN_COMB(done_bool_func());
    _PORT(bool) error_out = _ASSIGN_COMB(error_bool_func());

private:
    reg<u<clog2(Memory::MEM_DEPTH)>> addr_reg;
    reg<u1> write_reg;
    reg<logic<Memory::MEM_WIDTH_BYTES * 8>> write_data_reg;
    reg<logic<Memory::MEM_WIDTH_BYTES>> write_mask_reg;
    reg<u1> read_reg;
    reg<u<3>> state_reg;
    reg<u1> done_reg;
    reg<u1> error_reg;
    bool write_bool = false;
    bool read_bool = false;
    bool done_bool = false;
    bool error_bool = false;

    static constexpr logic<Memory::MEM_WIDTH_BYTES * 8> expected_word()
    {
        return logic<Memory::MEM_WIDTH_BYTES * 8>(0x12345678u);
    }

    bool& write_bool_func()
    {
        write_bool = static_cast<bool>(write_reg);
        return write_bool;
    }

    bool& read_bool_func()
    {
        read_bool = static_cast<bool>(read_reg);
        return read_bool;
    }

    bool& done_bool_func()
    {
        done_bool = static_cast<bool>(done_reg);
        return done_bool;
    }

    bool& error_bool_func()
    {
        error_bool = static_cast<bool>(error_reg);
        return error_bool;
    }

public:
    void _work(bool reset)
    {
        if (reset) {
            addr_reg.clr();
            write_reg.clr();
            write_data_reg.clr();
            write_mask_reg.clr();
            read_reg.clr();
            state_reg.clr();
            done_reg.clr();
            error_reg.clr();
            return;
        }

        addr_reg._next = addr_reg;
        write_reg._next = 0;
        write_data_reg._next = write_data_reg;
        write_mask_reg._next = logic<Memory::MEM_WIDTH_BYTES>(0xfu);
        read_reg._next = 0;
        state_reg._next = state_reg;
        done_reg._next = done_reg;
        error_reg._next = error_reg;

        switch (static_cast<unsigned>(state_reg)) {
        case 0:
            addr_reg._next = u<clog2(Memory::MEM_DEPTH)>(3);
            write_data_reg._next = expected_word();
            write_mask_reg._next = logic<Memory::MEM_WIDTH_BYTES>(0xfu);
            write_reg._next = 1;
            state_reg._next = u<3>(1);
            break;
        case 1:
            addr_reg._next = u<clog2(Memory::MEM_DEPTH)>(3);
            read_reg._next = 1;
            state_reg._next = u<3>(2);
            break;
        case 2:
            if (read_data_in() != expected_word()) {
                error_reg._next = 1;
            }
            done_reg._next = 1;
            state_reg._next = u<3>(3);
            break;
        default:
            done_reg._next = 1;
            break;
        }
    }

    void _strobe()
    {
        addr_reg.strobe();
        write_reg.strobe();
        write_data_reg.strobe();
        write_mask_reg.strobe();
        read_reg.strobe();
        state_reg.strobe();
        done_reg.strobe();
        error_reg.strobe();
    }

    void _assign() {}
};

class MemoryTestHarness : public Module
{
public:
#if defined(VERILATOR)
    VERILATOR_MODEL memory;
#else
    Memory memory;
#endif
    MemoryTest test;

    void _assign()
    {
#if !defined(VERILATOR)
        memory.addr_in = _ASSIGN_REG(test.addr_out());
        memory.write_in = _ASSIGN_REG(test.write_out());
        memory.write_data_in = _ASSIGN_REG(test.write_data_out());
        memory.write_mask_in = _ASSIGN_REG(test.write_mask_out());
        memory.read_in = _ASSIGN_REG(test.read_out());
        test.read_data_in = _ASSIGN_REG(memory.read_data_out());
        memory.__inst_name = __inst_name + "/memory";
        test.__inst_name = __inst_name + "/test";
        memory._assign();
        test._assign();
#endif
    }

    void _work(bool reset)
    {
#if defined(VERILATOR)
        memory.addr_in = test.addr_out();
        memory.write_in = test.write_out();
        std::memcpy(&memory.write_data_in, &test.write_data_out(), sizeof(memory.write_data_in));
        std::memcpy(&memory.write_mask_in, &test.write_mask_out(), sizeof(memory.write_mask_in));
        memory.read_in = test.read_out();
        memory.clk = 0;
        memory.reset = reset ? 1 : 0;
        memory.eval();
        test.read_data_in = _ASSIGN(*(logic<Memory::MEM_WIDTH_BYTES * 8>*)&memory.read_data_out);
        test._work(reset);
        memory.clk = 1;
        memory.eval();
#else
        test._work(reset);
        memory._work(reset);
#endif
    }

    void _strobe()
    {
#if !defined(VERILATOR)
        memory._strobe();
#endif
        test._strobe();
    }

    void _work_neg(bool reset)
    {
#if defined(VERILATOR)
        memory.clk = 0;
        memory.reset = reset ? 1 : 0;
        memory.eval();
#else
        (void)reset;
#endif
    }

    void _strobe_neg() {}

    bool passed()
    {
        return test.done_out() && !test.error_out();
    }
};
