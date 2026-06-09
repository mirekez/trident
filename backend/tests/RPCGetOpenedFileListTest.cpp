#include "RPCGetOpenedFileListTest.h"

std::string RPCGetOpenedFileListTest::sampleJson() {
    return R"({"title":"Opened Files","files":[{"path":"demo/top.cpp","language":"cpp","content":"#include <cstdint>\n\nnamespace demo {\nstruct Accumulator {\n    uint32_t value = 0;\n    void add(uint32_t next) { value += next; }\n};\n}\n"},{"path":"rtl/alu_slice.sv","language":"systemverilog","content":"module alu_slice #(parameter WIDTH = 32) (\n    input  logic [WIDTH-1:0] a,\n    input  logic [WIDTH-1:0] b,\n    input  logic [1:0] op,\n    output logic [WIDTH-1:0] y\n);\n    always_comb begin\n        unique case (op)\n            2'b00: y = a + b;\n            2'b01: y = a ^ b;\n            default: y = '0;\n        endcase\n    end\nendmodule\n"}],"mode":"test"})";
}

bool RPCGetOpenedFileListTest::validate() {
    const auto json = sampleJson();
    return json.find("\"files\"") != std::string::npos &&
           json.find("demo/top.cpp") != std::string::npos &&
           json.find("rtl/alu_slice.sv") != std::string::npos;
}
