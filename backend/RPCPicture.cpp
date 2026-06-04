#include "RPCPicture.h"

std::string RPCPicture::name() {
    return "picture";
}

std::string RPCPicture::handle() {
    return R"({"title":"Runtime Picture","mime":"image/svg+xml","image":"data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%22320%22%20height%3D%22180%22%20viewBox%3D%220%200%20320%20180%22%3E%3Crect%20width%3D%22320%22%20height%3D%22180%22%20fill%3D%22%23f8fafb%22%2F%3E%3Crect%20x%3D%2244%22%20y%3D%2242%22%20width%3D%22232%22%20height%3D%2296%22%20rx%3D%228%22%20fill%3D%22%23d9e1e8%22%20stroke%3D%22%23748694%22%2F%3E%3Ctext%20x%3D%2282%22%20y%3D%2298%22%20font-family%3D%22Arial%22%20font-size%3D%2220%22%20fill%3D%22%2320313f%22%3EReal%20RPC%20Picture%3C%2Ftext%3E%3C%2Fsvg%3E","mode":"real"})";
}
