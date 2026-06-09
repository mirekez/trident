#include "RPCGetOpenedFileList.h"

std::string RPCGetOpenedFileList::name() {
    return "get_opened_file_list";
}

std::string RPCGetOpenedFileList::handle() {
    return R"({"title":"Opened Files","files":[{"path":"src/main.cpp","language":"cpp","content":"#include <iostream>\n\nint main() {\n    std::cout << \"Development workspace\" << std::endl;\n    return 0;\n}\n"}],"mode":"real"})";
}
