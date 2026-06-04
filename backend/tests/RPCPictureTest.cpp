#include "RPCPictureTest.h"

std::string RPCPictureTest::sampleJson() {
    return R"({"title":"Generated Picture","mime":"image/svg+xml","image":"data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%22320%22%20height%3D%22180%22%20viewBox%3D%220%200%20320%20180%22%3E%3Crect%20width%3D%22320%22%20height%3D%22180%22%20fill%3D%22%23172a3a%22%2F%3E%3Ccircle%20cx%3D%2280%22%20cy%3D%2290%22%20r%3D%2248%22%20fill%3D%22%2327c2a0%22%2F%3E%3Crect%20x%3D%22152%22%20y%3D%2246%22%20width%3D%22112%22%20height%3D%2288%22%20rx%3D%2210%22%20fill%3D%22%23f6c85f%22%2F%3E%3Ctext%20x%3D%22160%22%20y%3D%22152%22%20font-family%3D%22Arial%22%20font-size%3D%2218%22%20fill%3D%22%23ffffff%22%3EC%2B%2B%20RPC%20Image%3C%2Ftext%3E%3C%2Fsvg%3E"})";
}

bool RPCPictureTest::validate() {
    const auto json = sampleJson();
    return json.find("\"mime\":\"image/svg+xml\"") != std::string::npos &&
           json.find("data:image/svg+xml") != std::string::npos;
}
