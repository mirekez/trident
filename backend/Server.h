#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

class Server {
public:
    struct HttpRequest {
        std::string method;
        std::string path;
    };

    explicit Server(unsigned short port, std::filesystem::path guiRoot = "gui");
    int run();

private:
    unsigned short port_;
    std::filesystem::path guiRoot_;
    std::unordered_map<std::string, std::string (*)()> rpcHandlers_;

    std::string dispatch(const HttpRequest& request) const;
    std::string serveStatic(const std::string& requestPath) const;
    std::string jsonResponse(int status, const std::string& body) const;
    std::string textResponse(int status, const std::string& contentType, const std::string& body) const;
    std::filesystem::path resolveGuiPath(const std::string& requestPath) const;
};
