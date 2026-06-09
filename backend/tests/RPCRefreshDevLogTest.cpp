#include "RPCRefreshDevLogTest.h"

std::string RPCRefreshDevLogTest::sampleJson() {
    return R"({"title":"Development Log","log":"[configure] CMake cache is ready.\n[build] Compiling backend/RPCGetOpenedFileListTest.cpp\n[build] Linking trident_backend_tests\n[test] RPC development payloads validated.\n","mode":"test"})";
}

bool RPCRefreshDevLogTest::validate() {
    const auto json = sampleJson();
    return json.find("\"log\"") != std::string::npos &&
           json.find("[build]") != std::string::npos;
}
