#include "RPCStatus.h"

std::string RPCStatus::name() {
    return "status";
}

std::string RPCStatus::handle() {
    return R"({"title":"Backend Status","state":"production-ready","uptimeSeconds":0,"services":["http","rpc"],"mode":"real"})";
}
