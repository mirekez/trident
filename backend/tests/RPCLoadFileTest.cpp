#include "RPCLoadFileTest.h"

std::string RPCLoadFileTest::sampleJson() {
    return R"({"title":"Mixed HDL/C++ Demo","path":"demo/mixed_design.cppv","language":"cpp","content":"// Mixed C++ and Verilog demo from LoadFile test RPC\n#include <cstdint>\n#include <array>\n\nnamespace trident {\nconstexpr uint32_t mask(uint32_t value, uint32_t bits) {\n    return value & ((1u << bits) - 1u);\n}\n\nstruct Packet {\n    uint32_t addr;\n    uint32_t data;\n    bool write;\n};\n}\n\nmodule alu_slice #(parameter WIDTH = 32) (\n    input  logic [WIDTH-1:0] a,\n    input  logic [WIDTH-1:0] b,\n    input  logic [1:0] op,\n    output logic [WIDTH-1:0] y\n);\n    always_comb begin\n        unique case (op)\n            2'b00: y = a + b;\n            2'b01: y = a ^ b;\n            default: y = '0;\n        endcase\n    end\nendmodule\n\nint main() {\n    trident::Packet pkt{0x1000, 0x55aa, true};\n    return pkt.write ? int(trident::mask(pkt.data, 8)) : 0;\n}\n","mode":"test"})";
}

bool RPCLoadFileTest::validate() {
    const auto json = sampleJson();
    return json.find("\"path\":\"demo/mixed_design.cppv\"") != std::string::npos &&
           json.find("module alu_slice") != std::string::npos &&
           json.find("namespace trident") != std::string::npos;
}
