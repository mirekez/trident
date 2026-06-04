#include "RPCStatusTest.h"

std::string RPCStatusTest::sampleJson() {
    return R"({"title":"Backend Status","state":"running","uptimeSeconds":42,"services":["http","rpc","static-gui"]})";
}

bool RPCStatusTest::validate() {
    const auto json = sampleJson();
    return json.find("\"state\":\"running\"") != std::string::npos &&
           json.find("\"services\"") != std::string::npos;
}
