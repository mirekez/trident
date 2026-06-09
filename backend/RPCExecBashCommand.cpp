#include "RPCExecBashCommand.h"

std::string RPCExecBashCommand::name() {
    return "exec_bash_command";
}

std::string RPCExecBashCommand::handle() {
    return R"({"title":"AI Agent Console","prompt":"trident$","output":"Command execution RPC is connected. Real command dispatch is intentionally stubbed for now.\n","mode":"real"})";
}
