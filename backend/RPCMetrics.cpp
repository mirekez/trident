#include "RPCMetrics.h"

std::string RPCMetrics::name() {
    return "metrics";
}

std::string RPCMetrics::handle() {
    return R"({"title":"Runtime Metrics","requestsToday":0,"latencyMs":[0,0,0,0,0],"activeUsers":0,"mode":"real"})";
}
