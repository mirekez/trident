#include "RPCRunBashCommand.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>

#if defined(_WIN32)
#define popen _popen
#define pclose _pclose
#endif

namespace {

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
        default:
            const auto byte = static_cast<unsigned char>(ch);
            if (byte < 0x20) {
                out << "\\u00";
                const char* digits = "0123456789abcdef";
                out << digits[(byte >> 4) & 0xf] << digits[byte & 0xf];
            } else {
                out << ch;
            }
            break;
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
        default: value += escaped; break;
        }
    }
    return value;
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::filesystem::path cwdFromRequest(const std::string& body, const std::filesystem::path& defaultCwd) {
    const auto requested = jsonStringValue(body, "cwd");
    if (requested.empty()) {
        return defaultCwd;
    }
    std::error_code error;
    auto path = std::filesystem::weakly_canonical(requested, error);
    if (error || !std::filesystem::is_directory(path, error)) {
        return defaultCwd;
    }
    return path;
}

std::string readPipe(const std::string& command) {
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "Failed to start bash process.\n";
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);
    return output;
}

} // namespace

std::string RPCRunBashCommand::name() {
    return "run_bash_command";
}

std::string RPCRunBashCommand::handle(const std::string& requestBody, const std::filesystem::path& defaultCwd) {
    const auto command = jsonStringValue(requestBody, "command");
    const auto cwd = cwdFromRequest(requestBody, defaultCwd);

    if (command.empty()) {
        return "{\"prompt\":\"trident$\",\"cwd\":\"" + jsonEscape(cwd.string()) +
               "\",\"output\":\"\",\"exitCode\":0,\"mode\":\"real\"}";
    }

    const std::string script =
        "cd " + shellQuote(cwd.string()) + " && {\n" + command +
        "\n}; code=$?; printf '\\n__TRIDENT_EXIT__:%d\\n__TRIDENT_CWD__:%s\\n' \"$code\" \"$PWD\"";
    std::string output = readPipe("bash -lc " + shellQuote(script) + " 2>&1");

    int exitCode = -1;
    std::string nextCwd = cwd.string();
    const auto exitMarker = output.rfind("\n__TRIDENT_EXIT__:");
    const auto cwdMarker = output.rfind("\n__TRIDENT_CWD__:");
    if (exitMarker != std::string::npos && cwdMarker != std::string::npos && cwdMarker > exitMarker) {
        const auto exitStart = exitMarker + std::string("\n__TRIDENT_EXIT__:").size();
        exitCode = std::stoi(output.substr(exitStart, cwdMarker - exitStart));
        const auto cwdStart = cwdMarker + std::string("\n__TRIDENT_CWD__:").size();
        const auto cwdEnd = output.find('\n', cwdStart);
        nextCwd = output.substr(cwdStart, cwdEnd == std::string::npos ? std::string::npos : cwdEnd - cwdStart);
        output.erase(exitMarker);
    }

    std::ostringstream json;
    json << "{\"prompt\":\"trident$\",\"cwd\":\"" << jsonEscape(nextCwd)
         << "\",\"output\":\"" << jsonEscape(output)
         << "\",\"exitCode\":" << exitCode
         << ",\"mode\":\"real\"}";
    return json.str();
}
