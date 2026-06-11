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
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
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
    case 409: return "Conflict";
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

bool jsonHasKey(const std::string& body, const std::string& key) {
    return body.find("\"" + key + "\"") != std::string::npos;
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

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        quoted += ch == '\'' ? "'\\''" : std::string(1, ch);
    }
    quoted += "'";
    return quoted;
}

std::string normalizedProjectName(std::string name) {
    constexpr std::string_view suffix = ".trident";
    if (name.size() > suffix.size() && name.ends_with(suffix)) {
        name.erase(name.size() - suffix.size());
    }
    return name;
}

bool validProjectName(const std::string& name) {
    return !name.empty() &&
           name != "." &&
           name != ".." &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos;
}

std::filesystem::path projectArchivePath(const Project& project) {
    return project.path / (project.projectName + ".trident");
}

bool archiveProject(Project& project, bool overwrite, std::string* message = nullptr) {
    if (!validProjectName(project.projectName)) {
        if (message) *message = "project_name_required";
        return false;
    }
    if (!project.save()) {
        if (message) *message = "save_project_failed";
        return false;
    }

    std::error_code error;
    const auto archive = projectArchivePath(project);
    if (std::filesystem::exists(archive, error) && !overwrite) {
        if (message) *message = "archive_exists";
        return false;
    }

    const auto command = "tar -cf " + shellQuote(archive.string()) +
                         " -C " + shellQuote(project.path.string()) + " .trident";
    const int rc = std::system(command.c_str());
    if (rc != 0) {
        if (message) *message = "archive_project_failed";
        return false;
    }
    return true;
}

bool extractProjectArchive(const std::filesystem::path& archive, Project& project, std::string* message = nullptr) {
    std::error_code error;
    auto archivePath = std::filesystem::weakly_canonical(archive, error);
    if (error || !std::filesystem::is_regular_file(archivePath, error) || archivePath.extension() != ".trident") {
        if (message) *message = "not_project_archive";
        return false;
    }

    const auto projectPath = archivePath.parent_path();
    std::filesystem::remove_all(projectPath / ".trident", error);
    if (error) {
        if (message) *message = "remove_existing_project_failed";
        return false;
    }

    const auto command = "tar -xf " + shellQuote(archivePath.string()) +
                         " -C " + shellQuote(projectPath.string());
    const int rc = std::system(command.c_str());
    if (rc != 0) {
        if (message) *message = "extract_project_failed";
        return false;
    }

    auto loaded = Project(projectPath);
    if (!Project::load(projectPath, loaded)) {
        if (message) *message = "load_project_failed";
        return false;
    }
    loaded.projectName = archivePath.stem().string();
    loaded.save();
    project = std::move(loaded);
    return true;
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

std::string filesystemListJson(const std::filesystem::path& root, const std::filesystem::path& path) {
    struct Item {
        std::filesystem::path path;
        bool directory = false;
        std::uintmax_t size = 0;
    };

    std::vector<Item> folders;
    std::vector<Item> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
        if (error) {
            break;
        }
        const auto name = entry.path().filename().string();
        if (name.starts_with(".")) {
            continue;
        }
        if (entry.is_directory(error)) {
            folders.push_back({entry.path(), true, 0});
        } else if (entry.is_regular_file(error)) {
            files.push_back({entry.path(), false, entry.file_size(error)});
        }
    }
    const auto byPath = [](const Item& lhs, const Item& rhs) { return lhs.path < rhs.path; };
    std::sort(folders.begin(), folders.end(), byPath);
    std::sort(files.begin(), files.end(), byPath);

    std::ostringstream json;
    json << "{\"root\":\"" << jsonEscape(root.string()) << "\",\"path\":\"" << jsonEscape(path.string())
         << "\",\"parent\":\"" << jsonEscape((path == root ? root : path.parent_path()).string()) << "\",\"items\":[";
    bool first = true;
    const auto writeItem = [&](const Item& item) {
        if (!first) {
            json << ',';
        }
        first = false;
        json << "{\"name\":\"" << jsonEscape(item.path.filename().string())
             << "\",\"path\":\"" << jsonEscape(item.path.string())
             << "\",\"kind\":\"" << (item.directory ? "folder" : "file")
             << "\",\"size\":" << item.size << "}";
    };
    for (const auto& folder : folders) {
        writeItem(folder);
    }
    for (const auto& file : files) {
        writeItem(file);
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

struct PushCommand {
    std::filesystem::path path;
    std::uint64_t id = 0;
    std::string json;
};

std::uint64_t pushCommandId(const std::string& name, std::string_view prefix) {
    constexpr std::string_view suffix = ".json";
    if (!name.starts_with(prefix) || !name.ends_with(suffix)) {
        return 0;
    }
    const auto idText = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    try {
        return std::stoull(idText);
    } catch (...) {
        return 0;
    }
}

std::vector<PushCommand> pendingPushCommands(const std::filesystem::path& pushRoot) {
    std::vector<PushCommand> commands;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(pushRoot, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file(error)) {
            continue;
        }
        const auto name = entry.path().filename().string();
        const auto body = readFile(entry.path());

        if (const auto id = pushCommandId(name, "pushSource_"); id != 0) {
            const auto filename = jsonStringValue(body, "filename");
            if (filename.empty()) {
                std::filesystem::remove(entry.path(), error);
                continue;
            }
            commands.push_back({
                entry.path(),
                id,
                "{\"action\":\"pushSource\",\"filename\":\"" + jsonEscape(filename) + "\"}\n"
            });
            continue;
        }

        if (const auto id = pushCommandId(name, "pushProjectSettings_"); id != 0) {
            commands.push_back({
                entry.path(),
                id,
                "{\"action\":\"pushProjectSettings\",\"topModuleName\":\"" +
                    jsonEscape(jsonStringValue(body, "topModuleName")) +
                    "\",\"topModuleFile\":\"" +
                    jsonEscape(jsonStringValue(body, "topModuleFile")) +
                    "\",\"mainTestFile\":\"" +
                    jsonEscape(jsonStringValue(body, "mainTestFile")) + "\"}\n"
            });
        }
    }
    std::sort(commands.begin(), commands.end(), [](const PushCommand& lhs, const PushCommand& rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        return lhs.path.filename().string() < rhs.path.filename().string();
    });
    return commands;
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
    : port_(port),
      testMode_(testMode),
      guiRoot_(std::move(guiRoot)),
      pushRoot_(std::filesystem::current_path() / ".push") {
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
        std::error_code pushError;
        std::filesystem::create_directories(pushRoot_, pushError);
        if (pushError) {
            std::cerr << "Warning: failed to create push directory " << pushRoot_
                      << ": " << pushError.message() << "\n";
        }
        writeAgentSkill(std::filesystem::current_path());

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
        std::cout << "GUI push directory: " << pushRoot_ << "\n";
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
        } else if (request.path == "/rpc/push-events") {
            streamPushEvents(rawClient);
        } else {
            const auto response = dispatch(request);
            send(client, response.data(), static_cast<int>(response.size()), 0);
        }
    }
    closeSocket(client);
}

void Server::streamPushEvents(std::intptr_t rawClient) {
    const auto client = static_cast<SocketHandle>(rawClient);
    const std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/x-ndjson; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n";
    if (send(client, header.data(), static_cast<int>(header.size()), 0) <= 0) {
        return;
    }

    while (keepRunning) {
        const auto commands = pendingPushCommands(pushRoot_);
        if (commands.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

        for (const auto& command : commands) {
            if (send(client, command.json.data(), static_cast<int>(command.json.size()), 0) <= 0) {
                return;
            }
            std::error_code error;
            std::filesystem::remove(command.path, error);
        }
    }
}

void Server::writeAgentSkill(const std::filesystem::path& root) const {
    std::error_code error;
    const auto agentsDir = root / ".agents";
    std::filesystem::create_directories(agentsDir, error);
    if (error) {
        std::cerr << "Warning: failed to create agent directory " << agentsDir
                  << ": " << error.message() << "\n";
        return;
    }

    std::ofstream file(agentsDir / "SKILLS.md", std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Warning: failed to write " << (agentsDir / "SKILLS.md") << "\n";
        return;
    }

    file
        << "# Trident GUI Push Commands\n\n"
        << "This project is written in cpphdl. Before editing project sources, read the cpphdl coding rules and headers in:\n\n"
        << "```text\n"
        << (compilationFlow_.toolsPath() / "cpphdl" / "include").string() << "\n"
        << "```\n\n"
        << "Backend variable values available to agents:\n\n"
        << "```text\n"
        << "$(BinDir)=" << pushRoot_.parent_path().string() << "\n"
        << "$(ToolsPath)=" << compilationFlow_.toolsPath().string() << "\n"
        << "cpphdl_tool=" << (compilationFlow_.toolsPath() / "cpphdl").string() << "\n"
        << "```\n\n"
        << "Use the backend push gateway to request GUI actions from scripts or agents.\n\n"
        << "Current push folder:\n\n"
        << "```text\n"
        << pushRoot_.string() << "\n"
        << "```\n\n"
        << "Create one JSON command file in that folder. The backend consumes the file, streams the command to the GUI, and deletes the file after sending it.\n\n"
        << "File naming:\n\n"
        << "```text\n"
        << "pushSource_<incremented_cmd_id>.json\n"
        << "pushProjectSettings_<incremented_cmd_id>.json\n"
        << "```\n\n"
        << "Use a monotonically increasing numeric command id. Example:\n\n"
        << "```text\n"
        << pushRoot_.string() << "/pushSource_1.json\n"
        << pushRoot_.string() << "/pushProjectSettings_2.json\n"
        << "```\n\n"
        << "Supported push functions:\n\n"
        << "- `pushSource(filename)`: asks the GUI to open `filename` in `Development -> EditorTabs`.\n"
        << "- `pushProjectSettings(topModuleName, topModuleFile, mainTestFile)`: asks the GUI to save project settings in the backend.\n\n"
        << "`pushSource` JSON format:\n\n"
        << "```json\n"
        << "{\"filename\":\"/absolute/path/to/source.cpp\"}\n"
        << "```\n\n"
        << "`pushProjectSettings` JSON format:\n\n"
        << "```json\n"
        << "{\"topModuleName\":\"Memory\",\"topModuleFile\":\"Memory.cpp\",\"mainTestFile\":\"MemoryTest.cpp\"}\n"
        << "```\n\n"
        << "Parameter rules:\n\n"
        << "- `filename` must be an absolute or project-root-accessible source path.\n"
        << "- The file must be readable by the backend and allowed by the active project root.\n"
        << "- The Development window must be open in the GUI; otherwise the GUI ignores the command and reports status.\n\n"
        << "After generating the cpphdl model source and the main test source, push the project settings with `pushProjectSettings` so Trident knows the top module name, top module file, and main test file.\n\n"
        << "Shell example:\n\n"
        << "```sh\n"
        << "printf '{\"filename\":\"/path/to/file.cpp\"}\\n' > "
        << pushRoot_.string() << "/pushSource_1.json\n"
        << "printf '{\"topModuleName\":\"Memory\",\"topModuleFile\":\"Memory.cpp\",\"mainTestFile\":\"MemoryTest.cpp\"}\\n' > "
        << pushRoot_.string() << "/pushProjectSettings_2.json\n"
        << "```\n";
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
        request.path == "/rpc/close-project" ||
        request.path == "/rpc/get-project-settings" ||
        request.path == "/rpc/save-project-settings" ||
        request.path == "/rpc/get-current-project-dir" ||
        request.path == "/rpc/get-opened-file-list" ||
        request.path == "/rpc/update-development-tabs" ||
        request.path == "/rpc/list-filesystem" ||
        request.path == "/rpc/create-filesystem-file" ||
        request.path == "/rpc/delete-filesystem-file" ||
        request.path == "/rpc/rename-filesystem-file" ||
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
    if (!project_) {
        return jsonResponse(500, R"({"error":"no_project"})");
    }
    if (project_->topModuleName.empty() || project_->topModuleFile.empty()) {
        return jsonResponse(500, R"({"error":"compile_settings_required"})");
    }

    return jsonResponse(200, compilationFlow_.execute(
        "Compile",
        project_->path,
        pushRoot_.parent_path(),
        project_->projectName,
        project_->topModuleName,
        project_->topModuleFile,
        project_->mainTestFile));
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
        project_->save();
        std::filesystem::current_path(project_->path, error);
        writeAgentSkill(project_->path);
        return jsonResponse(200, "{\"project\":" + project_->toJson() + ",\"created\":true}");
    }

    if (request.path == "/rpc/load-project") {
        const auto archive = jsonStringValue(request.body, "path");
        Project loaded(std::filesystem::current_path());
        std::string message;
        if (!extractProjectArchive(archive, loaded, &message)) {
            return jsonResponse(500, "{\"error\":\"" + jsonEscape(message) + "\"}");
        }
        project_ = std::make_unique<Project>(std::move(loaded));
        std::error_code error;
        std::filesystem::current_path(project_->path, error);
        writeAgentSkill(project_->path);
        return jsonResponse(200, "{\"project\":" + project_->toJson() + ",\"loaded\":true}");
    }

    if (request.path == "/rpc/save-project") {
        if (!project_) {
            return jsonResponse(500, R"({"error":"no_project"})");
        }
        const auto requestedName = normalizedProjectName(jsonStringValue(request.body, "projectName"));
        if (!requestedName.empty()) {
            project_->projectName = requestedName;
        }
        const bool overwrite = jsonBoolValue(request.body, "overwrite");
        std::string message;
        if (!archiveProject(*project_, overwrite, &message)) {
            const int status = message == "archive_exists" ? 409 : 500;
            return jsonResponse(status, "{\"error\":\"" + jsonEscape(message) + "\"}");
        }
        return jsonResponse(200, "{\"project\":" + project_->toJson() + ",\"archive\":\"" +
            jsonEscape(projectArchivePath(*project_).string()) + "\",\"saved\":true}");
    }

    if (request.path == "/rpc/close-project") {
        project_.reset();
        return jsonResponse(200, R"({"closed":true})");
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
        if (jsonHasKey(request.body, "topModuleName")) {
            project_->topModuleName = jsonStringValue(request.body, "topModuleName");
        }
        if (jsonHasKey(request.body, "topModuleFile")) {
            project_->topModuleFile = jsonStringValue(request.body, "topModuleFile");
        }
        if (jsonHasKey(request.body, "mainTestFile")) {
            project_->mainTestFile = jsonStringValue(request.body, "mainTestFile");
        }
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
        return jsonResponse(200, openedFilesJson(project_.get()));
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

    if (request.path == "/rpc/list-filesystem") {
        const auto root = projectRoot();
        const auto requested = jsonStringValue(request.body, "path");
        const auto path = canonicalInsideRoot(root, requested.empty() ? root : std::filesystem::path(requested));
        std::error_code error;
        if (!std::filesystem::is_directory(path, error)) {
            return jsonResponse(500, R"({"error":"not_a_directory"})");
        }
        return jsonResponse(200, filesystemListJson(root, path));
    }

    if (request.path == "/rpc/create-filesystem-file") {
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
        return jsonResponse(200, "{\"created\":true,\"directory\":" + filesystemListJson(root, dir) + "}");
    }

    if (request.path == "/rpc/delete-filesystem-file") {
        const auto root = projectRoot();
        const auto path = canonicalInsideRoot(root, jsonStringValue(request.body, "path"));
        std::error_code error;
        const bool isFile = std::filesystem::is_regular_file(path, error);
        const bool isDirectory = std::filesystem::is_directory(path, error);
        if (!isFile && !isDirectory) {
            return jsonResponse(500, R"({"error":"not_a_file_or_directory"})");
        }
        const auto parent = path.parent_path();
        if (isDirectory) {
            std::filesystem::remove_all(path, error);
        } else {
            std::filesystem::remove(path, error);
        }
        if (error) {
            return jsonResponse(500, "{\"error\":\"delete_failed\",\"message\":\"" + jsonEscape(error.message()) + "\"}");
        }
        if (project_ && isFile) {
            removeOpenedFile(*project_, path);
        }
        return jsonResponse(200, "{\"deleted\":true,\"directory\":" + filesystemListJson(root, parent) + "}");
    }

    if (request.path == "/rpc/rename-filesystem-file") {
        const auto root = projectRoot();
        const auto path = canonicalInsideRoot(root, jsonStringValue(request.body, "path"));
        const auto name = jsonStringValue(request.body, "name");
        if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
            return jsonResponse(500, R"({"error":"invalid_file_name"})");
        }
        std::error_code error;
        const bool isFile = std::filesystem::is_regular_file(path, error);
        const bool isDirectory = std::filesystem::is_directory(path, error);
        if (!isFile && !isDirectory) {
            return jsonResponse(500, R"({"error":"not_a_file_or_directory"})");
        }
        const auto target = canonicalInsideRoot(root, path.parent_path() / name);
        if (std::filesystem::exists(target, error)) {
            return jsonResponse(500, R"({"error":"file_exists"})");
        }
        std::filesystem::rename(path, target, error);
        if (error) {
            return jsonResponse(500, "{\"error\":\"rename_file_failed\",\"message\":\"" + jsonEscape(error.message()) + "\"}");
        }
        if (project_ && isFile) {
            removeOpenedFile(*project_, path);
            addOpenedFile(*project_, target);
        }
        return jsonResponse(200, "{\"renamed\":true,\"directory\":" + filesystemListJson(root, target.parent_path()) + "}");
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
