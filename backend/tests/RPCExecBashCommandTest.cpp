#include "RPCExecBashCommandTest.h"

std::string RPCExecBashCommandTest::sampleJson() {
    return R"({"title":"AI Agent Console","prompt":"trident-test$","output":"test agent accepted command\nready for codex or claude integration\n","mode":"test"})";
}

bool RPCExecBashCommandTest::validate() {
    const auto json = sampleJson();
    return json.find("\"prompt\":\"trident-test$\"") != std::string::npos &&
           json.find("ready for codex") != std::string::npos;
}
