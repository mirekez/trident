#include "Project.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
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
        case '\\': value += '\\'; break;
        case '"': value += '"'; break;
        default: value += escaped; break;
        }
    }
    return value;
}

} // namespace

Project::Project(std::filesystem::path projectPath)
    : path(std::filesystem::weakly_canonical(std::move(projectPath))) {
}

bool Project::load(const std::filesystem::path& projectPath, Project& project) {
    std::error_code error;
    const auto canonicalPath = std::filesystem::weakly_canonical(projectPath, error);
    const auto metadata = canonicalPath / ".trident" / "project.json";
    std::ifstream file(metadata, std::ios::binary);
    if (!file) {
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    const auto json = stream.str();
    project = Project(canonicalPath);
    const auto topModuleName = jsonStringValue(json, "topModuleName");
    if (!topModuleName.empty()) {
        project.topModuleName = topModuleName;
    }
    std::size_t pos = 0;
    while ((pos = json.find("\"path\":\"", pos)) != std::string::npos) {
        pos += 8;
        const auto end = json.find('"', pos);
        if (end == std::string::npos) {
            break;
        }
        const auto value = json.substr(pos, end - pos);
        if (value != canonicalPath.string()) {
            project.openedFiles.push_back(std::filesystem::path(value));
        }
        pos = end + 1;
    }
    return true;
}

std::string Project::toJson() const {
    std::ostringstream json;
    json << "{\"path\":\"" << jsonEscape(path.string()) << "\",\"topModuleName\":\""
         << jsonEscape(topModuleName) << "\",\"openedFiles\":[";
    for (std::size_t i = 0; i < openedFiles.size(); ++i) {
        if (i != 0) {
            json << ',';
        }
        json << "{\"path\":\"" << jsonEscape(openedFiles[i].string()) << "\"}";
    }
    json << "]}";
    return json.str();
}

bool Project::save() const {
    std::error_code error;
    const auto tridentDir = path / ".trident";
    std::filesystem::create_directories(tridentDir, error);
    if (error) {
        return false;
    }

    std::ofstream file(tridentDir / "project.json", std::ios::binary);
    if (!file) {
        return false;
    }

    file << toJson() << "\n";
    return true;
}
