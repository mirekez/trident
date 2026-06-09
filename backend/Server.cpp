#include "Server.h"

#include "RPCMetrics.h"
#include "RPCPicture.h"
#include "RPCStatus.h"
#include "RPCExecBashCommand.h"
#include "RPCExecBashCommandTest.h"
#include "RPCGetOpenedFileList.h"
#include "RPCGetOpenedFileListTest.h"
#include "RPCLoadFile.h"
#include "RPCLoadFileTest.h"
#include "RPCMetricsTest.h"
#include "RPCPictureTest.h"
#include "RPCRefreshDevLog.h"
#include "RPCRefreshDevLogTest.h"
#include "RPCRunBashCommand.h"
#include "RPCStatusTest.h"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
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

std::size_t contentLength(const std::string& raw) {
    const auto headerPos = raw.find("Content-Length:");
    if (headerPos == std::string::npos) {
        return 0;
    }
    const auto valueStart = headerPos + std::string("Content-Length:").size();
    const auto valueEnd = raw.find("\r\n", valueStart);
    const auto value = raw.substr(valueStart, valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
    try {
        return static_cast<std::size_t>(std::stoul(value));
    } catch (...) {
        return 0;
    }
}

bool hasFullBody(const std::string& raw) {
    const auto bodyPos = raw.find("\r\n\r\n");
    if (bodyPos == std::string::npos) {
        return false;
    }
    return raw.size() >= bodyPos + 4 + contentLength(raw);
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
}

std::string jsonStringValue(const std::string& body, const std::string& key) {
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return {};
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return {};
    }
    const auto firstQuote = body.find('"', colon + 1);
    if (firstQuote == std::string::npos) {
        return {};
    }

    std::string value;
    for (std::size_t i = firstQuote + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (ch == '"') {
            return value;
        }
        if (ch != '\\' || i + 1 >= body.size()) {
            value += ch;
            continue;
        }

        const char escaped = body[++i];
        switch (escaped) {
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        case 'b': value += '\b'; break;
        case 'f': value += '\f'; break;
        case '\\': value += '\\'; break;
        case '"': value += '"'; break;
        case 'u':
            if (i + 4 < body.size()) {
                const auto hex = body.substr(i + 1, 4);
                char* end = nullptr;
                const auto code = std::strtoul(hex.c_str(), &end, 16);
                if (end == hex.c_str() + 4 && code <= 0x7f) {
                    value += static_cast<char>(code);
                    i += 4;
                }
            }
            break;
        default: value += escaped; break;
        }
    }
    return value;
}

bool jsonBoolValue(const std::string& body, const std::string& key) {
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return false;
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return false;
    }
    const auto valueStart = body.find_first_not_of(" \t\r\n", colon + 1);
    return valueStart != std::string::npos && body.compare(valueStart, 4, "true") == 0;
}

std::filesystem::path requestPathOrCurrent(const std::string& body) {
    std::error_code error;
    const auto requested = jsonStringValue(body, "path");
    auto path = requested.empty() ? std::filesystem::current_path() : std::filesystem::path(requested);
    path = std::filesystem::weakly_canonical(path, error);
    if (error || !std::filesystem::is_directory(path, error)) {
        return std::filesystem::current_path();
    }
    return path;
}

std::filesystem::path canonicalInsideRoot(const std::filesystem::path& root, const std::filesystem::path& requested) {
    std::error_code error;
    auto path = requested.is_absolute() ? requested : root / requested;
    path = std::filesystem::weakly_canonical(path, error);
    if (error) {
        path = requested.is_absolute() ? requested : root / requested;
        path = std::filesystem::absolute(path, error);
    }
    const auto rootString = std::filesystem::weakly_canonical(root, error).string();
    const auto pathString = path.string();
    if (pathString == rootString || pathString.rfind(rootString + std::string(1, std::filesystem::path::preferred_separator), 0) == 0) {
        return path;
    }
    return root;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::string languageForPath(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".sv" || ext == ".v") return "systemverilog";
    if (ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") return "cpp";
    return "text";
}

void addOpenedFile(Project& project, const std::filesystem::path& path) {
    if (std::find(project.openedFiles.begin(), project.openedFiles.end(), path) == project.openedFiles.end()) {
        project.openedFiles.push_back(path);
    }
}

void removeOpenedFile(Project& project, const std::filesystem::path& path) {
    project.openedFiles.erase(std::remove(project.openedFiles.begin(), project.openedFiles.end(), path), project.openedFiles.end());
}

std::string openedFilesJson(const Project* project) {
    std::ostringstream json;
    json << "{\"files\":[";
    if (project) {
        bool first = true;
        for (const auto& path : project->openedFiles) {
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error)) {
                continue;
            }
            if (!first) {
                json << ',';
            }
            first = false;
            json << "{\"path\":\"" << jsonEscape(path.string()) << "\",\"language\":\""
                 << jsonEscape(languageForPath(path)) << "\",\"content\":\"" << jsonEscape(readTextFile(path)) << "\"}";
        }
    }
    json << "]}";
    return json.str();
}

std::string folderListJson(const std::filesystem::path& path) {
    std::vector<std::filesystem::path> folders;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
        if (!error && entry.is_directory(error) && !entry.path().filename().string().starts_with(".")) {
            folders.push_back(entry.path());
        }
    }
    std::sort(folders.begin(), folders.end());

    std::ostringstream json;
    json << "{\"path\":\"" << jsonEscape(path.string()) << "\",\"parent\":\""
         << jsonEscape(path.parent_path().string()) << "\",\"folders\":[";
    for (std::size_t i = 0; i < folders.size(); ++i) {
        if (i != 0) {
            json << ',';
        }
        json << "{\"name\":\"" << jsonEscape(folders[i].filename().string()) << "\",\"path\":\""
             << jsonEscape(folders[i].string()) << "\"}";
    }
    json << "]}";
    return json.str();
}

std::string projectFileListJson(const std::filesystem::path& root, const std::filesystem::path& path) {
    std::vector<std::filesystem::path> folders;
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
        if (error) {
            break;
        }
        if (entry.is_directory(error)) {
            if (!entry.path().filename().string().starts_with(".")) {
                folders.push_back(entry.path());
            }
        } else if (entry.is_regular_file(error)) {
            files.push_back(entry.path());
        }
    }
    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());

    std::ostringstream json;
    json << "{\"root\":\"" << jsonEscape(root.string()) << "\",\"path\":\"" << jsonEscape(path.string())
         << "\",\"parent\":\"" << jsonEscape((path == root ? root : path.parent_path()).string()) << "\",\"folders\":[";
    for (std::size_t i = 0; i < folders.size(); ++i) {
        if (i != 0) json << ',';
        json << "{\"name\":\"" << jsonEscape(folders[i].filename().string()) << "\",\"path\":\"" << jsonEscape(folders[i].string()) << "\"}";
    }
    json << "],\"files\":[";
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (i != 0) json << ',';
        json << "{\"name\":\"" << jsonEscape(files[i].filename().string()) << "\",\"path\":\"" << jsonEscape(files[i].string()) << "\"}";
    }
    json << "]}";
    return json.str();
}

std::filesystem::path findDefaultFlowPath() {
    std::error_code error;
    const auto cwd = std::filesystem::current_path(error);
    const std::array candidates = {
        cwd / "default_flow.json",
        cwd / "build" / "default_flow.json",
        cwd.parent_path() / "default_flow.json"
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, error)) {
            return candidate;
        }
    }
    return cwd / "default_flow.json";
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
    const auto bodyPos = raw.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        request.body = raw.substr(bodyPos + 4);
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
    const auto flowPath = findDefaultFlowPath();
    if (!compilationFlow_.load(flowPath)) {
        std::cerr << "Warning: failed to load compilation flow from " << flowPath << "\n";
    }

    if (testMode_) {
        rpcHandlers_.emplace("/rpc/status", &RPCStatusTest::sampleJson);
        rpcHandlers_.emplace("/rpc/metrics", &RPCMetricsTest::sampleJson);
        rpcHandlers_.emplace("/rpc/picture", &RPCPictureTest::sampleJson);
        rpcHandlers_.emplace("/rpc/load-file", &RPCLoadFileTest::sampleJson);
        rpcHandlers_.emplace("/rpc/get-opened-file-list", &RPCGetOpenedFileListTest::sampleJson);
        rpcHandlers_.emplace("/rpc/refresh-dev-log", &RPCRefreshDevLogTest::sampleJson);
        rpcHandlers_.emplace("/rpc/exec-bash-command", &RPCExecBashCommandTest::sampleJson);
    } else {
        rpcHandlers_.emplace("/rpc/status", &RPCStatus::handle);
        rpcHandlers_.emplace("/rpc/metrics", &RPCMetrics::handle);
        rpcHandlers_.emplace("/rpc/picture", &RPCPicture::handle);
        rpcHandlers_.emplace("/rpc/load-file", &RPCLoadFile::handle);
        rpcHandlers_.emplace("/rpc/get-opened-file-list", &RPCGetOpenedFileList::handle);
        rpcHandlers_.emplace("/rpc/refresh-dev-log", &RPCRefreshDevLog::handle);
        rpcHandlers_.emplace("/rpc/exec-bash-command", &RPCExecBashCommand::handle);
    }
}

int Server::run() {
    try {
        WinsockSession winsock;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);
#if !defined(_WIN32)
        std::signal(SIGPIPE, SIG_IGN);
#endif

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

            std::thread(&Server::handleClient, this, static_cast<std::intptr_t>(client)).detach();
        }

        closeSocket(listener);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << "\n";
        return 1;
    }
}

void Server::handleClient(std::intptr_t rawClient) {
    const auto client = static_cast<SocketHandle>(rawClient);
    std::array<char, 8192> buffer{};
    const int received = recv(client, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
    if (received > 0) {
        std::string raw(buffer.data(), static_cast<std::size_t>(received));
        while (!hasFullBody(raw) && raw.size() < 1024 * 1024) {
            const int more = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (more <= 0) {
                break;
            }
            raw.append(buffer.data(), static_cast<std::size_t>(more));
        }
        const auto request = parseRequest(raw);
        if (request.path == "/rpc/bash-console-stream") {
            streamBashConsole(rawClient);
        } else {
            const auto response = dispatch(request);
            send(client, response.data(), static_cast<int>(response.size()), 0);
        }
    }
    closeSocket(client);
}

void Server::streamBashConsole(std::intptr_t rawClient) {
    const auto client = static_cast<SocketHandle>(rawClient);
    bashConsole_.start(projectRoot());
    const std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n";
    if (send(client, header.data(), static_cast<int>(header.size()), 0) <= 0) {
        return;
    }

    while (keepRunning && bashConsole_.running()) {
        auto output = bashConsole_.readAvailable(200);
        if (output.empty()) {
            continue;
        }
        if (send(client, output.data(), static_cast<int>(output.size()), 0) <= 0) {
            break;
        }
    }
}

std::string Server::dispatch(const HttpRequest& request) {
    if (request.method == "OPTIONS") {
        return textResponse(204, "text/plain", "");
    }
    if (request.method != "GET" && request.method != "POST" && request.method != "HEAD") {
        return jsonResponse(405, R"({"error":"method_not_allowed"})");
    }

    if (request.path == "/rpc/run-bash-command") {
        return jsonResponse(200, RPCRunBashCommand::handle(request.body, projectRoot()));
    }
    if (request.path == "/rpc/bash-console-input") {
        const auto input = jsonStringValue(request.body, "input");
        const bool ok = bashConsole_.writeInput(input);
        return jsonResponse(ok ? 200 : 500, ok ? R"({"sent":true})" : R"({"error":"console_not_running"})");
    }
    if (request.path == "/rpc/bash-console-stop") {
        bashConsole_.stop();
        return jsonResponse(200, R"({"stopped":true})");
    }
    if (request.path == "/rpc/compile") {
        return handleCompileRpc();
    }
    if (request.path == "/rpc/list-folders" ||
        request.path == "/rpc/create-folder" ||
        request.path == "/rpc/create-project" ||
        request.path == "/rpc/load-project" ||
        request.path == "/rpc/save-project" ||
        request.path == "/rpc/get-project-settings" ||
        request.path == "/rpc/save-project-settings" ||
        request.path == "/rpc/get-current-project-dir" ||
        request.path == "/rpc/get-opened-file-list" ||
        request.path == "/rpc/update-development-tabs" ||
        request.path == "/rpc/list-project-files" ||
        request.path == "/rpc/open-file" ||
        request.path == "/rpc/create-file" ||
        request.path == "/rpc/close-file" ||
        request.path == "/rpc/save-file") {
        return handleProjectRpc(request);
    }

    if (const auto handler = rpcHandlers_.find(request.path); handler != rpcHandlers_.end()) {
        return jsonResponse(200, handler->second());
    }

    return serveStatic(request.path);
}

std::string Server::handleCompileRpc() {
    if (!compilationFlow_.loaded()) {
        return jsonResponse(500, R"({"error":"flow_not_loaded"})");
    }
    return jsonResponse(200, compilationFlow_.execute(
        "Compile",
        projectRoot(),
        project_ ? project_->topModuleName : std::string("top")));
}

std::string Server::handleProjectRpc(const HttpRequest& request) {
    if (request.path == "/rpc/list-folders") {
        return jsonResponse(200, folderListJson(requestPathOrCurrent(request.body)));
    }

    if (request.path == "/rpc/create-folder") {
        const auto parent = requestPathOrCurrent(request.body);
        const auto name = jsonStringValue(request.body, "name");
        if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
            return jsonResponse(500, R"({"error":"invalid_folder_name"})");
        }
        std::error_code error;
        const auto created = parent / name;
        std::filesystem::create_directories(created, error);
        if (error) {
            return jsonResponse(500, "{\"error\":\"create_folder_failed\",\"message\":\"" + jsonEscape(error.message()) + "\"}");
        }
        return jsonResponse(200, folderListJson(std::filesystem::weakly_canonical(created, error)));
    }

    if (request.path == "/rpc/create-project") {
        const auto path = requestPathOrCurrent(request.body);
        std::error_code error;
        const auto tridentDir = path / ".trident";
        if (std::filesystem::exists(tridentDir, error)) {
            if (!jsonBoolValue(request.body, "overwrite")) {
                return jsonResponse(409, R"({"error":"project_exists"})");
            }
            std::filesystem::remove_all(tridentDir, error);
            if (error) {
                return jsonResponse(500, "{\"error\":\"remove_existing_project_failed\",\"message\":\"" + jsonEscape(error.message()) + "\"}");
            }
        }
        project_ = std::make_unique<Project>(path);
        std::filesystem::current_path(project_->path, error);
        return jsonResponse(200, "{\"project\":" + project_->toJson() + ",\"created\":true}");
    }

    if (request.path == "/rpc/load-project") {
        const auto path = requestPathOrCurrent(request.body);
        if (!std::filesystem::exists(path / ".trident" / "project.json")) {
            return jsonResponse(500, R"({"error":"No project folder specified"})");
        }
        auto loaded = Project(path);
        if (!Project::load(path, loaded)) {
            return jsonResponse(500, R"({"error":"No project folder specified"})");
        }
        project_ = std::make_unique<Project>(std::move(loaded));
        std::error_code error;
        std::filesystem::current_path(project_->path, error);
        return jsonResponse(200, "{\"project\":" + project_->toJson() + ",\"loaded\":true}");
    }

    if (request.path == "/rpc/save-project") {
        if (!project_) {
            return jsonResponse(500, R"({"error":"no_project"})");
        }
        if (!project_->save()) {
            return jsonResponse(500, R"({"error":"save_project_failed"})");
        }
        return jsonResponse(200, "{\"project\":" + project_->toJson() + ",\"saved\":true}");
    }

    if (request.path == "/rpc/get-project-settings") {
        if (!project_) {
            return jsonResponse(500, R"({"error":"no_project"})");
        }
        return jsonResponse(200, "{\"settings\":" + project_->toJson() + "}");
    }

    if (request.path == "/rpc/save-project-settings") {
        if (!project_) {
            return jsonResponse(500, R"({"error":"no_project"})");
        }
        const auto topModuleName = jsonStringValue(request.body, "topModuleName");
        project_->topModuleName = topModuleName.empty() ? "top" : topModuleName;
        if (!project_->save()) {
            return jsonResponse(500, R"({"error":"save_project_failed"})");
        }
        return jsonResponse(200, "{\"settings\":" + project_->toJson() + ",\"saved\":true}");
    }

    if (request.path == "/rpc/get-current-project-dir") {
        return jsonResponse(200, "{\"path\":\"" + jsonEscape(projectRoot().string()) + "\",\"hasProject\":" + std::string(project_ ? "true" : "false") + "}");
    }

    if (request.path == "/rpc/update-development-tabs") {
        return jsonResponse(200, openedFilesJson(project_.get()));
    }

    if (request.path == "/rpc/get-opened-file-list") {
        return jsonResponse(200, project_ ? openedFilesJson(project_.get()) : RPCGetOpenedFileListTest::sampleJson());
    }

    if (request.path == "/rpc/list-project-files") {
        const auto root = projectRoot();
        const auto requested = jsonStringValue(request.body, "path");
        const auto path = canonicalInsideRoot(root, requested.empty() ? root : std::filesystem::path(requested));
        std::error_code error;
        if (!std::filesystem::is_directory(path, error)) {
            return jsonResponse(500, R"({"error":"not_a_directory"})");
        }
        return jsonResponse(200, projectFileListJson(root, path));
    }

    if (request.path == "/rpc/open-file") {
        const auto root = projectRoot();
        const auto path = canonicalInsideRoot(root, jsonStringValue(request.body, "path"));
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            return jsonResponse(500, R"({"error":"not_a_file"})");
        }
        if (project_) {
            addOpenedFile(*project_, path);
        }
        return jsonResponse(200, "{\"path\":\"" + jsonEscape(path.string()) + "\",\"language\":\"" +
            jsonEscape(languageForPath(path)) + "\",\"content\":\"" + jsonEscape(readTextFile(path)) + "\"}");
    }

    if (request.path == "/rpc/create-file") {
        const auto root = projectRoot();
        const auto dir = canonicalInsideRoot(root, jsonStringValue(request.body, "path"));
        const auto name = jsonStringValue(request.body, "name");
        if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
            return jsonResponse(500, R"({"error":"invalid_file_name"})");
        }
        std::error_code error;
        if (!std::filesystem::is_directory(dir, error)) {
            return jsonResponse(500, R"({"error":"not_a_directory"})");
        }
        const auto filePath = canonicalInsideRoot(root, dir / name);
        if (std::filesystem::exists(filePath, error)) {
            return jsonResponse(500, R"({"error":"file_exists"})");
        }
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            return jsonResponse(500, R"({"error":"create_file_failed"})");
        }
        if (project_) {
            addOpenedFile(*project_, filePath);
        }
        return jsonResponse(200, "{\"path\":\"" + jsonEscape(filePath.string()) + "\",\"language\":\"" +
            jsonEscape(languageForPath(filePath)) + "\",\"content\":\"\"}");
    }

    if (request.path == "/rpc/close-file") {
        if (project_) {
            const auto path = canonicalInsideRoot(projectRoot(), jsonStringValue(request.body, "path"));
            removeOpenedFile(*project_, path);
        }
        return jsonResponse(200, R"({"closed":true})");
    }

    if (request.path == "/rpc/save-file") {
        const auto root = projectRoot();
        const auto path = canonicalInsideRoot(root, jsonStringValue(request.body, "path"));
        const auto content = jsonStringValue(request.body, "content");
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            return jsonResponse(500, R"({"error":"not_a_file"})");
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            return jsonResponse(500, R"({"error":"save_file_failed"})");
        }
        file << content;
        return jsonResponse(200, "{\"path\":\"" + jsonEscape(path.string()) + "\",\"saved\":true}");
    }

    return jsonResponse(404, R"({"error":"unknown_project_rpc"})");
}

std::filesystem::path Server::projectRoot() const {
    return project_ ? project_->path : std::filesystem::current_path();
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
