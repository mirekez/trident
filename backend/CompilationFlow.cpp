#include "CompilationFlow.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

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
            if (static_cast<unsigned char>(ch) < 0x20) {
                out << "\\u00";
                constexpr char digits[] = "0123456789abcdef";
                const auto byte = static_cast<unsigned char>(ch);
                out << digits[(byte >> 4) & 0xf] << digits[byte & 0xf];
            } else {
                out << ch;
            }
            break;
        }
    }
    return out.str();
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

std::string stripLineComments(const std::string& text) {
    std::string result;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (!inString && ch == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
            if (i < text.size()) {
                result += text[i];
            }
            continue;
        }
        result += ch;
        if (escaped) {
            escaped = false;
        } else if (ch == '\\' && inString) {
            escaped = true;
        } else if (ch == '"') {
            inString = !inString;
        }
    }
    return result;
}

std::string parseJsonString(const std::string& text, std::size_t quotePos, std::size_t* endPos = nullptr) {
    std::string value;
    for (std::size_t i = quotePos + 1; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '"') {
            if (endPos) {
                *endPos = i + 1;
            }
            return value;
        }
        if (ch != '\\' || i + 1 >= text.size()) {
            value += ch;
            continue;
        }
        const char escaped = text[++i];
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

std::string parseJsonStringArray(const std::string& text, std::size_t arrayPos, std::size_t* endPos = nullptr) {
    std::ostringstream value;
    bool first = true;
    std::size_t pos = arrayPos + 1;
    while (pos < text.size()) {
        pos = text.find_first_not_of(" \t\r\n,", pos);
        if (pos == std::string::npos) {
            break;
        }
        if (text[pos] == ']') {
            if (endPos) {
                *endPos = pos + 1;
            }
            return value.str();
        }
        if (text[pos] != '"') {
            break;
        }
        std::size_t stringEnd = 0;
        if (!first) {
            value << '\n';
        }
        first = false;
        value << parseJsonString(text, pos, &stringEnd);
        pos = stringEnd;
    }
    return value.str();
}

std::unordered_map<std::string, std::string> parseActionCommands(const std::string& text) {
    std::unordered_map<std::string, std::string> commands;
    const auto actionsKey = text.find("\"actions\"");
    if (actionsKey == std::string::npos) {
        return commands;
    }
    auto pos = text.find('{', actionsKey);
    if (pos == std::string::npos) {
        return commands;
    }
    ++pos;
    while (pos < text.size()) {
        pos = text.find('"', pos);
        if (pos == std::string::npos) {
            break;
        }
        std::size_t actionEnd = 0;
        const auto action = parseJsonString(text, pos, &actionEnd);
        const auto commandKey = text.find("\"command\"", actionEnd);
        const auto nextAction = text.find("\n    \"", actionEnd);
        if (commandKey == std::string::npos || (nextAction != std::string::npos && nextAction < commandKey)) {
            pos = actionEnd;
            continue;
        }
        const auto colon = text.find(':', commandKey);
        const auto valueStart = colon == std::string::npos ? std::string::npos : text.find_first_not_of(" \t\r\n", colon + 1);
        if (valueStart == std::string::npos) {
            pos = actionEnd;
            continue;
        }
        std::size_t commandEnd = 0;
        if (text[valueStart] == '"') {
            commands[action] = parseJsonString(text, valueStart, &commandEnd);
        } else if (text[valueStart] == '[') {
            commands[action] = parseJsonStringArray(text, valueStart, &commandEnd);
        } else {
            pos = actionEnd;
            continue;
        }
        pos = commandEnd;
    }
    return commands;
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        quoted += ch == '\'' ? "'\\''" : std::string(1, ch);
    }
    quoted += "'";
    return quoted;
}

bool isCSource(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" ||
           ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx";
}

bool isSvSource(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    return ext == ".v" || ext == ".sv" || ext == ".vh" || ext == ".svh";
}

std::string sourceList(const std::filesystem::path& projectPath, bool (*predicate)(const std::filesystem::path&)) {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    if (!std::filesystem::exists(projectPath, error)) {
        return {};
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(projectPath, error)) {
        if (error) {
            break;
        }
        const auto filename = entry.path().filename().string();
        if (filename.starts_with(".")) {
            if (entry.is_directory(error)) {
                continue;
            }
        }
        if (entry.is_regular_file(error) && predicate(entry.path())) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::ostringstream list;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        if (i != 0) {
            list << ' ';
        }
        list << shellQuote(paths[i].string());
    }
    return list.str();
}

std::filesystem::path existingToolsPath(const std::filesystem::path& flowPath) {
    std::error_code error;
    const auto cwd = std::filesystem::current_path(error);
    const auto flowRoot = flowPath.empty() ? cwd : flowPath.parent_path();
    const std::array candidates = {
        flowRoot / "build" / "tools" / "build-current",
        flowRoot / "tools" / "build-current",
        cwd / "tools" / "build-current",
        cwd / "build" / "tools" / "build-current",
        cwd.parent_path() / "build" / "tools" / "build-current",
        flowRoot / "build" / "tools",
        flowRoot / "tools",
        cwd / "tools",
        cwd.parent_path() / "tools"
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, error)) {
            return std::filesystem::weakly_canonical(candidate, error);
        }
    }
    return cwd / "tools" / "build-current";
}

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
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

bool CompilationFlow::load(const std::filesystem::path& path) {
    const auto text = stripLineComments(readTextFile(path));
    auto parsed = parseActionCommands(text);
    if (parsed.empty()) {
        return false;
    }
    commands_ = std::move(parsed);
    sourcePath_ = path;
    return true;
}

bool CompilationFlow::loaded() const {
    return !commands_.empty();
}

const std::filesystem::path& CompilationFlow::sourcePath() const {
    return sourcePath_;
}

std::filesystem::path CompilationFlow::toolsPath() const {
    return existingToolsPath(sourcePath_);
}

bool CompilationFlow::hasAction(const std::string& action) const {
    return commands_.find(action) != commands_.end();
}

std::string CompilationFlow::execute(const std::string& action,
                                     const std::filesystem::path& projectPath,
                                     const std::filesystem::path& binDir,
                                     const std::string& projectName,
                                     const std::string& topModuleName,
                                     const std::string& topModuleFile,
                                     const std::string& mainTestFile,
                                     const std::string& additionalSources,
                                     const std::string& jsonOutputPath) const {
    const auto commandIt = commands_.find(action);
    if (commandIt == commands_.end()) {
        return "{\"error\":\"unknown_flow_action\",\"action\":\"" + jsonEscape(action) + "\"}";
    }

    std::error_code error;
    const auto project = std::filesystem::weakly_canonical(projectPath, error);
    const auto projectString = error ? projectPath.string() : project.string();
    const auto toolsPath = this->toolsPath().string();

    auto command = commandIt->second;
    replaceAll(command, "$(ToolsPath)", toolsPath);
    replaceAll(command, "$(BinDir)", binDir.string());
    replaceAll(command, "$(ProjectPath)", projectString);
    replaceAll(command, "$(ProjectName)", projectName);
    replaceAll(command, "$(CSourceList)", sourceList(projectString, isCSource));
    replaceAll(command, "$(SVSourceList)", sourceList(projectString, isSvSource));
    replaceAll(command, "$(TopModuleName)", topModuleName);
    replaceAll(command, "${TopModuleName}", topModuleName);
    replaceAll(command, "$(TopModuleFile)", topModuleFile);
    replaceAll(command, "${TopModuleFile}", topModuleFile);
    replaceAll(command, "$(MainTestFile)", mainTestFile);
    replaceAll(command, "${MainTestFile}", mainTestFile);
    replaceAll(command, "$(AdditionalSources)", additionalSources);
    replaceAll(command, "${AdditionalSources}", additionalSources);
    replaceAll(command, "$(JsonOutputPath)", jsonOutputPath);
    replaceAll(command, "${JsonOutputPath}", jsonOutputPath);

    const std::string script =
        "cd " + shellQuote(projectString) + " && {\n" + command +
        "\n}; code=$?; printf '\\n__TRIDENT_EXIT__:%d\\n' \"$code\"";
    auto output = readPipe("bash -lc " + shellQuote(script) + " 2>&1");

    int exitCode = -1;
    const auto marker = output.rfind("\n__TRIDENT_EXIT__:");
    if (marker != std::string::npos) {
        const auto start = marker + std::string("\n__TRIDENT_EXIT__:").size();
        try {
            exitCode = std::stoi(output.substr(start));
        } catch (...) {
            exitCode = -1;
        }
        output.erase(marker);
    }

    std::ostringstream json;
    json << "{\"action\":\"" << jsonEscape(action)
         << "\",\"flow\":\"" << jsonEscape(sourcePath_.string())
         << "\",\"command\":\"" << jsonEscape(command)
         << "\",\"output\":\"" << jsonEscape(output)
         << "\",\"exitCode\":" << exitCode << "}";
    return json.str();
}
