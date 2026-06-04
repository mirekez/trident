#include "RPCLoadFile.h"

std::string RPCLoadFile::name() {
    return "load_file";
}

std::string RPCLoadFile::handle() {
    return R"({"title":"Loaded File","path":"src/main.cpp","language":"cpp","content":"#include <iostream>\n\nint main() {\n    std::cout << \"LoadFile real RPC is connected\" << std::endl;\n    return 0;\n}\n","mode":"real"})";
}
