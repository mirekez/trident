#include "RPCRefreshDevLog.h"

std::string RPCRefreshDevLog::name() {
    return "refresh_dev_log";
}

std::string RPCRefreshDevLog::handle() {
    return R"({"title":"Development Log","log":"[build] No active build.\n[cmake] Waiting for source changes.\n","mode":"real"})";
}
