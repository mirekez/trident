#pragma once

#include <filesystem>
#include <mutex>
#include <string>

class BashConsoleSession {
public:
    BashConsoleSession();
    ~BashConsoleSession();

    bool start(const std::filesystem::path& cwd);
    bool running();
    bool writeInput(const std::string& input);
    std::string readAvailable(int timeoutMs);
    void stop();

private:
    std::mutex mutex_;
    int masterFd_ = -1;
    int childPid_ = -1;
};
