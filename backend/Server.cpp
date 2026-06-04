#include "Server.h"

#include "RPCMetrics.h"
#include "RPCPicture.h"
#include "RPCStatus.h"
#include "RPCLoadFile.h"
#include "RPCLoadFileTest.h"
#include "RPCMetricsTest.h"
#include "RPCPictureTest.h"
#include "RPCStatusTest.h"

#include <array>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle invalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle invalidSocket = -1;
#endif

namespace {

volatile std::sig_atomic_t keepRunning = 1;

void handleSignal(int) {
    keepRunning = 0;
}

void closeSocket(SocketHandle socket) {
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

std::string reasonPhrase(int status) {
    switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "Unknown";
    }
}

std::string contentTypeFor(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

Server::HttpRequest parseRequest(const std::string& raw) {
    std::istringstream stream(raw);
    Server::HttpRequest request;
    stream >> request.method >> request.path;
    if (request.path.empty()) {
        request.path = "/";
    }
    const auto queryPos = request.path.find('?');
    if (queryPos != std::string::npos) {
        request.path.erase(queryPos);
    }
    return request;
}

class WinsockSession {
public:
    WinsockSession() {
#if defined(_WIN32)
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~WinsockSession() {
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};

} // namespace

Server::Server(unsigned short port, bool testMode, std::filesystem::path guiRoot)
    : port_(port), testMode_(testMode), guiRoot_(std::move(guiRoot)) {
    if (testMode_) {
        rpcHandlers_.emplace("/rpc/status", &RPCStatusTest::sampleJson);
        rpcHandlers_.emplace("/rpc/metrics", &RPCMetricsTest::sampleJson);
        rpcHandlers_.emplace("/rpc/picture", &RPCPictureTest::sampleJson);
        rpcHandlers_.emplace("/rpc/load-file", &RPCLoadFileTest::sampleJson);
    } else {
        rpcHandlers_.emplace("/rpc/status", &RPCStatus::handle);
        rpcHandlers_.emplace("/rpc/metrics", &RPCMetrics::handle);
        rpcHandlers_.emplace("/rpc/picture", &RPCPicture::handle);
        rpcHandlers_.emplace("/rpc/load-file", &RPCLoadFile::handle);
    }
}

int Server::run() {
    try {
        WinsockSession winsock;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        SocketHandle listener = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listener == invalidSocket) {
            std::cerr << "Failed to create socket.\n";
            return 1;
        }

        int opt = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port_);

        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            std::cerr << "Failed to bind to 127.0.0.1:" << port_ << ".\n";
            closeSocket(listener);
            return 1;
        }

        if (listen(listener, 16) != 0) {
            std::cerr << "Failed to listen on socket.\n";
            closeSocket(listener);
            return 1;
        }

        std::cout << "Backend listening at http://127.0.0.1:" << port_;
        if (testMode_) {
            std::cout << " in test RPC mode";
        }
        std::cout << "\n";
        while (keepRunning) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listener, &readSet);
            timeval timeout{};
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            const int ready = select(static_cast<int>(listener + 1), &readSet, nullptr, nullptr, &timeout);
            if (ready <= 0) {
                continue;
            }

            sockaddr_in clientAddress{};
#if defined(_WIN32)
            int clientSize = sizeof(clientAddress);
#else
            socklen_t clientSize = sizeof(clientAddress);
#endif
            SocketHandle client = accept(listener, reinterpret_cast<sockaddr*>(&clientAddress), &clientSize);
            if (client == invalidSocket) {
                continue;
            }

            std::array<char, 8192> buffer{};
            const int received = recv(client, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
            if (received > 0) {
                const auto request = parseRequest(std::string(buffer.data(), static_cast<std::size_t>(received)));
                const auto response = dispatch(request);
                send(client, response.data(), static_cast<int>(response.size()), 0);
            }
            closeSocket(client);
        }

        closeSocket(listener);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << "\n";
        return 1;
    }
}

std::string Server::dispatch(const HttpRequest& request) const {
    if (request.method == "OPTIONS") {
        return textResponse(204, "text/plain", "");
    }
    if (request.method != "GET" && request.method != "POST" && request.method != "HEAD") {
        return jsonResponse(405, R"({"error":"method_not_allowed"})");
    }

    if (const auto handler = rpcHandlers_.find(request.path); handler != rpcHandlers_.end()) {
        return jsonResponse(200, handler->second());
    }

    return serveStatic(request.path);
}

std::string Server::serveStatic(const std::string& requestPath) const {
    const auto path = resolveGuiPath(requestPath);
    if (path.empty() || !std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return textResponse(404, "text/plain; charset=utf-8", "Not found");
    }
    return textResponse(200, contentTypeFor(path), readFile(path));
}

std::string Server::jsonResponse(int status, const std::string& body) const {
    return textResponse(status, "application/json; charset=utf-8", body);
}

std::string Server::textResponse(int status, const std::string& contentType, const std::string& body) const {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reasonPhrase(status) << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

std::filesystem::path Server::resolveGuiPath(const std::string& requestPath) const {
    if (requestPath.find("..") != std::string::npos) {
        return {};
    }

    const std::string relative = requestPath == "/" ? "index.html" : requestPath.substr(1);
    const std::array candidates = {
        guiRoot_,
        std::filesystem::current_path() / "gui",
        std::filesystem::current_path().parent_path() / "gui"
    };

    for (const auto& root : candidates) {
        auto path = root / relative;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return guiRoot_ / relative;
}
