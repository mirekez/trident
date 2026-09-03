#include "RPCGenerateClassTest.h"

#include "RPCGenerateClass.h"

#include <iostream>
#include <string>

namespace {

std::string expectedGeneratedDemo() {
    return R"(#pragma once

#include "cpphdl.h"

using namespace cpphdl;

extern long _system_clock;

#if defined(VERILATOR)
#define TRIDENT_STRINGIFY_IMPL(name) #name
#define TRIDENT_STRINGIFY(name) TRIDENT_STRINGIFY_IMPL(name)
#define TRIDENT_VERILATOR_HEADER(name) TRIDENT_STRINGIFY(name.h)
#include TRIDENT_VERILATOR_HEADER(VERILATOR_MODEL)
#endif

struct GeneratedDemoStruct
{
    logic<32> data = 0;

    constexpr static size_t _size_bits() { return 32; }
    logic<32> pack() const { return data; }
    GeneratedDemoStruct& operator=(const logic<32>& value)
    {
        data = value;
        return *this;
    }
};

class GeneratedDemoInterface : public Interface
{
public:
    logic<32> payload = 0;

    constexpr static size_t _size_bits() { return 32; }
    logic<32> pack() const { return payload; }
    GeneratedDemoInterface& operator=(const logic<32>& value)
    {
        payload = value;
        return *this;
    }
    void _assign() {}
};

class GeneratedDemo : public Module
{
public:
    _PORT(i32) signed_value_in;
    _PORT(u<16>) unsigned_value_in;
    _PORT(logic<12>) logic_value_in;
    _PORT(GeneratedDemoStruct) struct_value_in;
    _PORT(array<i32, 3, false>) int_items_in;
    _PORT(array<u<8>, 4, true>) uint_items_in;
    _PORT(array<logic<7>, 5, true>) logic_items_out = _ASSIGN_COMB(logic_items_out_value);
    _PORT(array<GeneratedDemoStruct, 2, false>) struct_items_out = _ASSIGN_COMB(struct_items_out_value);
    _PORT(GeneratedDemoInterface) bus_if_in;

private:
    array<logic<7>, 5, true> logic_items_out_value{};
    array<GeneratedDemoStruct, 2, false> struct_items_out_value{};
    i32 signed_state{};
    reg<u<16>> unsigned_count_reg;
    logic<12> logic_state{};
    GeneratedDemoStruct packet_struct{};
    array<i32, 3, false> int_buffer{};
    reg<array<u<8>, 4, true>> uint_buffer_reg;
    _LAZY_COMB(logic_buffer_comb, array<logic<7>, 5, true>)
        logic_buffer_comb = {};
        return logic_buffer_comb;
    }
    array<GeneratedDemoStruct, 2, false> struct_buffer{};
    GeneratedDemoInterface local_if{};

public:
    void _work(bool reset)
    {
        (void)reset;
        unsigned_count_reg._next = unsigned_count_reg;
        uint_buffer_reg._next = uint_buffer_reg;
    }

    void _work_neg(bool reset)
    {
        (void)reset;
    }

    void _strobe()
    {
        unsigned_count_reg.strobe();
        uint_buffer_reg.strobe();
    }

    void _assign() {}
};

#if !defined(SYNTHESIS)
class GeneratedDemoTestHarness : public Module
{
public:
#if defined(VERILATOR)
    VERILATOR_MODEL dut;
#else
    GeneratedDemo dut;
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

    void _work_neg(bool reset)
    {
#if defined(VERILATOR)
        dut.clk = 1;
        dut.reset = reset ? 1 : 0;
        dut.eval();
#else
        dut._work_neg(reset);
#endif
    }

    void _strobe_neg()
    {
#if !defined(VERILATOR)
        dut._strobe_neg();
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
    GeneratedDemoTestHarness harness;
    harness._assign();
    harness._work(true);
    harness._work_neg(true);
    harness._strobe();
    ++_system_clock;
    harness._work(false);
    harness._work_neg(false);
    harness._strobe();
    return 0;
}
#endif
#endif
)";
}

bool contains(const std::string& body, const std::string& needle) {
    if (body.find(needle) != std::string::npos) {
        return true;
    }
    std::cerr << "Missing generated class fragment: " << needle << "\n";
    return false;
}

} // namespace

bool RPCGenerateClassTest::validate() {
    const auto request = RPCGenerateClass::sampleRequest();
    const auto result = RPCGenerateClass::generate(request);
    if (!result.ok) {
        std::cerr << "RPCGenerateClass failed: " << result.error << "\n";
        return false;
    }

    bool ok = true;
    if (result.content != expectedGeneratedDemo()) {
        std::cerr << "GeneratedDemo reference mismatch.\n";
        ok = false;
    }
    ok = contains(result.content, "class GeneratedDemo : public Module") && ok;
    ok = contains(result.content, "struct GeneratedDemoStruct") && ok;
    ok = contains(result.content, "class GeneratedDemoInterface : public Interface") && ok;
    ok = contains(result.content, "_PORT(i32) signed_value_in;") && ok;
    ok = contains(result.content, "_PORT(u<16>) unsigned_value_in;") && ok;
    ok = contains(result.content, "_PORT(logic<12>) logic_value_in;") && ok;
    ok = contains(result.content, "_PORT(GeneratedDemoStruct) struct_value_in;") && ok;
    ok = contains(result.content, "_PORT(array<i32, 3, false>) int_items_in;") && ok;
    ok = contains(result.content, "_PORT(array<u<8>, 4, true>) uint_items_in;") && ok;
    ok = contains(result.content, "_PORT(array<logic<7>, 5, true>) logic_items_out = _ASSIGN_COMB(logic_items_out_value);") && ok;
    ok = contains(result.content, "_PORT(array<GeneratedDemoStruct, 2, false>) struct_items_out = _ASSIGN_COMB(struct_items_out_value);") && ok;
    ok = contains(result.content, "_PORT(GeneratedDemoInterface) bus_if_in;") && ok;
    ok = contains(result.content, "reg<u<16>> unsigned_count_reg;") && ok;
    ok = contains(result.content, "reg<array<u<8>, 4, true>> uint_buffer_reg;") && ok;
    ok = contains(result.content, "_LAZY_COMB(logic_buffer_comb, array<logic<7>, 5, true>)") && ok;
    ok = contains(result.content, "void _work_neg(bool reset)") && ok;
    ok = contains(result.content, "dut.clk = 1;") && ok;
    ok = contains(result.content, "class GeneratedDemoTestHarness : public Module") && ok;
    ok = contains(result.content, "#include TRIDENT_VERILATOR_HEADER(VERILATOR_MODEL)") && ok;
    ok = contains(RPCGenerateClass::defaultsJson(true), "\"className\":\"GeneratedDemo\"") && ok;

    auto bad = request;
    bad.className = "1Bad";
    ok = !RPCGenerateClass::generate(bad).ok && ok;
    bad = request;
    bad.ports.front().name = "bad";
    ok = !RPCGenerateClass::generate(bad).ok && ok;
    bad = request;
    bad.ports.front().name = "clk_in";
    ok = !RPCGenerateClass::generate(bad).ok && ok;
    bad = request;
    bad.ports.front().name = "reset_in";
    ok = !RPCGenerateClass::generate(bad).ok && ok;
    bad = request;
    bad.members.front().combinational = true;
    bad.members.front().name = "notcomb";
    ok = !RPCGenerateClass::generate(bad).ok && ok;

    return ok;
}
