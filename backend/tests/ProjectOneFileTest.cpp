#include "ProjectOneFileTest.h"

#include "Project.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::filesystem::path uniqueTestDir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("trident_one_file_project_test_" + std::to_string(stamp));
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

bool ProjectOneFileTest::validate() {
    const auto root = uniqueTestDir();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) {
        std::cerr << "ProjectOneFileTest: failed to create temp dir\n";
        return false;
    }

    const auto cleanup = [&]() {
        std::error_code cleanupError;
        std::filesystem::remove_all(root, cleanupError);
    };

    const auto source = root / "Memory.cpp";
    {
        std::ofstream file(source, std::ios::binary);
        file << "#include \"cpphdl.h\"\nclass Memory {};\n";
    }

    Project project(root);
    if (!Project::loadOneFile(source, project)) {
        cleanup();
        std::cerr << "ProjectOneFileTest: loadOneFile failed\n";
        return false;
    }

    const auto metadata = root / ".tribe" / "Memory.json";
    const auto json = readTextFile(metadata);
    const bool ok = project.singleFileProject &&
        project.path == root &&
        project.projectFile == source &&
        project.projectName == "Memory" &&
        project.topModuleName == "Memory" &&
        project.topModuleFile == "Memory.cpp" &&
        project.mainTestFile == "Memory.cpp" &&
        project.openedFiles.size() == 1 &&
        project.openedFiles.front() == source &&
        std::filesystem::is_regular_file(metadata, error) &&
        contains(json, "\"singleFileProject\":true") &&
        contains(json, "\"projectFile\":\"" + source.string() + "\"") &&
        contains(json, "\"topModuleFile\":\"Memory.cpp\"") &&
        contains(json, "\"openedFiles\":[{\"path\":\"" + source.string() + "\"}]");

    if (!ok) {
        std::cerr << "ProjectOneFileTest failed\n";
        std::cerr << project.toJson() << "\n";
        std::cerr << json << "\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}
