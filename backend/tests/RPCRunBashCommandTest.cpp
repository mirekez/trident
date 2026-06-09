#include "RPCRunBashCommandTest.h"

#include "RPCRunBashCommand.h"

#include <string>

bool RPCRunBashCommandTest::validate() {
    const auto json = RPCRunBashCommand::handle(R"({"command":"printf trident-run-bash","cwd":"."})");
    return json.find("trident-run-bash") != std::string::npos &&
           json.find("\"exitCode\":0") != std::string::npos &&
           json.find("\"cwd\"") != std::string::npos;
}
