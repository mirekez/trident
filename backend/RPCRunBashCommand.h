#pragma once

#include <filesystem>
#include <string>

class RPCRunBashCommand {
public:
    static std::string name();
    static std::string handle(const std::string& requestBody, const std::filesystem::path& defaultCwd = std::filesystem::current_path());
};
