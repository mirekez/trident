#pragma once

#include "cpphdl.h"

#include <iostream>

using namespace cpphdl;

class Memory : public Module
{
public:
    static constexpr size_t MEM_WIDTH_BYTES = 4;
    static constexpr size_t MEM_DEPTH = 16;
    static constexpr bool SHOWAHEAD = true;

    _PORT(u<clog2(MEM_DEPTH)>) addr_in;
    _PORT(bool) write_in;
    _PORT(logic<MEM_WIDTH_BYTES * 8>) write_data_in;
    _PORT(logic<MEM_WIDTH_BYTES>) write_mask_in;
    _PORT(bool) read_in;
    _PORT(logic<MEM_WIDTH_BYTES * 8>) read_data_out = _ASSIGN_COMB(read_data_comb_func());

    bool debugen_in = false;

private:
    reg<logic<MEM_WIDTH_BYTES * 8>> read_data_reg;
    memory<u8, MEM_WIDTH_BYTES, MEM_DEPTH> buffer;
    logic<MEM_WIDTH_BYTES * 8> read_data_comb;

    logic<MEM_WIDTH_BYTES * 8>& read_data_comb_func()
    {
        if (SHOWAHEAD) {
            read_data_comb = buffer[addr_in()];
        } else {
            read_data_comb = read_data_reg;
        }
        return read_data_comb;
    }

public:
    void _work(bool reset)
    {
        if (reset) {
            read_data_reg.clr();
        } else {
            if (write_in()) {
                buffer[addr_in()] = write_data_in();
            }

            if (!SHOWAHEAD && read_in()) {
                read_data_reg._next = buffer[addr_in()];
            }
        }
    }

    void _strobe()
    {
        buffer.apply();
        read_data_reg.strobe();
    }

    void _assign() {}
};

using DemoMemory = Memory;
