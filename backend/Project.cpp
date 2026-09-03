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

std::string jsonArrayValue(const std::string& body, const std::string& key) {
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return {};
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return {};
    }
    const auto arrayStart = body.find('[', colon + 1);
    if (arrayStart == std::string::npos) {
        return {};
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (std::size_t i = arrayStart; i < body.size(); ++i) {
        const char ch = body[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (inString && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return body.substr(arrayStart, i - arrayStart + 1);
            }
        }
    }
    return {};
}

bool isCppFile(const std::filesystem::path& path) {
    return path.extension() == ".cpp";
}

std::filesystem::path oneFileMetadataPath(const std::filesystem::path& filePath) {
    return filePath.parent_path() / ".tribe" / (filePath.stem().string() + ".json");
}

} // namespace

Project::Project(std::filesystem::path projectPath)
    : path(std::filesystem::weakly_canonical(std::move(projectPath))),
      mainTestFile("main.cpp") {
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
    project.projectName = jsonStringValue(json, "projectName");
    const auto topModuleName = jsonStringValue(json, "topModuleName");
    if (!topModuleName.empty()) {
        project.topModuleName = topModuleName;
    }
    project.topModuleFile = jsonStringValue(json, "topModuleFile");
    const auto mainTestFile = jsonStringValue(json, "mainTestFile");
    if (!mainTestFile.empty()) {
        project.mainTestFile = mainTestFile;
    }
    project.additionalSources = jsonStringValue(json, "additionalSources");
    const auto windowsJson = jsonArrayValue(json, "windows");
    if (!windowsJson.empty()) {
        project.windowsJson = windowsJson;
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

Project Project::createOneFile(const std::filesystem::path& filePath) {
    std::error_code error;
    const auto canonicalFile = std::filesystem::weakly_canonical(filePath, error);
    const auto file = error ? std::filesystem::absolute(filePath) : canonicalFile;

    Project project(file.parent_path());
    project.singleFileProject = true;
    project.projectFile = file;
    project.projectName = file.stem().string();
    project.topModuleName = file.stem().string();
    project.topModuleFile = file.filename().string();
    project.mainTestFile = file.filename().string();
    project.additionalSources.clear();
    project.openedFiles = {file};
    project.windowsJson = R"([{"key":"development","open":true,"x":88,"y":88,"width":1120,"height":724,"maximized":false,"alwaysOnTop":false}])";
    return project;
}

bool Project::loadOneFile(const std::filesystem::path& filePath, Project& project) {
    std::error_code error;
    const auto canonicalFile = std::filesystem::weakly_canonical(filePath, error);
    if (error || !std::filesystem::is_regular_file(canonicalFile, error) || !isCppFile(canonicalFile)) {
        return false;
    }

    auto loaded = createOneFile(canonicalFile);
    std::ifstream file(oneFileMetadataPath(canonicalFile), std::ios::binary);
    if (file) {
        std::ostringstream stream;
        stream << file.rdbuf();
        const auto json = stream.str();

        const auto projectName = jsonStringValue(json, "projectName");
        if (!projectName.empty()) {
            loaded.projectName = projectName;
        }
        const auto topModuleName = jsonStringValue(json, "topModuleName");
        if (!topModuleName.empty()) {
            loaded.topModuleName = topModuleName;
        }
        const auto topModuleFile = jsonStringValue(json, "topModuleFile");
        if (!topModuleFile.empty()) {
            loaded.topModuleFile = topModuleFile;
        }
        const auto mainTestFile = jsonStringValue(json, "mainTestFile");
        if (!mainTestFile.empty()) {
            loaded.mainTestFile = mainTestFile;
        }
        loaded.additionalSources = jsonStringValue(json, "additionalSources");
        const auto windowsJson = jsonArrayValue(json, "windows");
        if (!windowsJson.empty()) {
            loaded.windowsJson = windowsJson;
        }
    }

    loaded.singleFileProject = true;
    loaded.projectFile = canonicalFile;
    loaded.openedFiles = {canonicalFile};
    loaded.save();
    project = std::move(loaded);
    return true;
}

std::string Project::toJson() const {
    std::ostringstream json;
    json << "{\"path\":\"" << jsonEscape(path.string()) << "\",\"projectName\":\""
         << jsonEscape(projectName) << "\",\"singleFileProject\":"
         << (singleFileProject ? "true" : "false") << ",\"projectFile\":\""
         << jsonEscape(projectFile.string()) << "\",\"topModuleName\":\""
         << jsonEscape(topModuleName) << "\",\"topModuleFile\":\""
         << jsonEscape(topModuleFile) << "\",\"mainTestFile\":\""
         << jsonEscape(mainTestFile) << "\",\"additionalSources\":\""
         << jsonEscape(additionalSources) << "\",\"windows\":"
         << (windowsJson.empty() ? "[]" : windowsJson) << ",\"openedFiles\":[";
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
    const auto metadata = singleFileProject && !projectFile.empty()
        ? oneFileMetadataPath(projectFile)
        : path / ".trident" / "project.json";
    std::filesystem::create_directories(metadata.parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream file(metadata, std::ios::binary);
    if (!file) {
        return false;
    }

    file << toJson() << "\n";
    return true;
}
