#pragma once

#include <filesystem>
#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "BashConsoleSession.h"
#include "CompilationFlow.h"
#include "Project.h"

class Server {
public:
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string body;
    };

    explicit Server(unsigned short port, bool testMode = false, std::filesystem::path guiRoot = "gui");
    int run();

private:
    unsigned short port_;
    bool testMode_;
    std::filesystem::path guiRoot_;
    std::unordered_map<std::string, std::function<std::string()>> rpcHandlers_;

    std::unique_ptr<Project> project_;
    BashConsoleSession bashConsole_;
    CompilationFlow compilationFlow_;

    void handleClient(std::intptr_t client);
    void streamBashConsole(std::intptr_t client);
    std::string dispatch(const HttpRequest& request);
    std::string handleProjectRpc(const HttpRequest& request);
    std::string handleCompileRpc();
    std::filesystem::path projectRoot() const;
    std::string serveStatic(const std::string& requestPath) const;
    std::string jsonResponse(int status, const std::string& body) const;
    std::string textResponse(int status, const std::string& contentType, const std::string& body) const;
    std::filesystem::path resolveGuiPath(const std::string& requestPath) const;
};
