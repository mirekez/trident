#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Project {
    std::filesystem::path path;
    std::vector<std::filesystem::path> openedFiles;
    std::string topModuleName = "top";

    explicit Project(std::filesystem::path projectPath);

    static bool load(const std::filesystem::path& projectPath, Project& project);
    std::string toJson() const;
    bool save() const;
};
