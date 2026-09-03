#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

class CompilationFlow {
public:
    bool load(const std::filesystem::path& path);
    bool loaded() const;
    const std::filesystem::path& sourcePath() const;
    std::filesystem::path toolsPath() const;
    bool hasAction(const std::string& action) const;
    std::string execute(const std::string& action,
                        const std::filesystem::path& projectPath,
                        const std::filesystem::path& binDir,
                        const std::string& projectName,
                        const std::string& topModuleName,
                        const std::string& topModuleFile,
                        const std::string& mainTestFile,
                        const std::string& additionalSources,
                        const std::string& jsonOutputPath = {}) const;

private:
    std::filesystem::path sourcePath_;
    std::unordered_map<std::string, std::string> commands_;
};
