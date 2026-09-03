#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Project {
    std::filesystem::path path;
    std::string projectName;
    bool singleFileProject = false;
    std::filesystem::path projectFile;
    std::vector<std::filesystem::path> openedFiles;
    std::string topModuleName;
    std::string topModuleFile;
    std::string mainTestFile;
    std::string additionalSources;
    std::string windowsJson = "[]";

    explicit Project(std::filesystem::path projectPath);

    static bool load(const std::filesystem::path& projectPath, Project& project);
    static bool loadOneFile(const std::filesystem::path& filePath, Project& project);
    static Project createOneFile(const std::filesystem::path& filePath);
    std::string toJson() const;
    bool save() const;
};
