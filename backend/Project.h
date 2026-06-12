#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Project {
    std::filesystem::path path;
    std::string projectName;
    std::vector<std::filesystem::path> openedFiles;
    std::string topModuleName;
    std::string topModuleFile;
    std::string mainTestFile;
    std::string additionalSources;
    std::string windowsJson = "[]";

    explicit Project(std::filesystem::path projectPath);

    static bool load(const std::filesystem::path& projectPath, Project& project);
    std::string toJson() const;
    bool save() const;
};
