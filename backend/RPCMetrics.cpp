#include "RPCMetrics.h"

#include "RPCMetricsTest.h"

std::string RPCMetrics::name() {
    return "metrics";
}

std::string RPCMetrics::handle() {
    return RPCMetricsTest::sampleJson();
}
