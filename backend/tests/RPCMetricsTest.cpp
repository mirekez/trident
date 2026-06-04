#include "RPCMetricsTest.h"

std::string RPCMetricsTest::sampleJson() {
    return R"({"title":"Demo Metrics","requestsToday":128,"latencyMs":[12,19,16,23,18],"activeUsers":7})";
}

bool RPCMetricsTest::validate() {
    const auto json = sampleJson();
    return json.find("\"requestsToday\":128") != std::string::npos &&
           json.find("\"latencyMs\"") != std::string::npos;
}
