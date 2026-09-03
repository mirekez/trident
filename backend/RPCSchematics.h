#pragma once

#include <filesystem>
#include <string>

class CompilationFlow;

class RPCSchematics {
public:
    struct Result {
        bool ok = false;
        std::string json;
        std::string error;
    };

    static Result loadAndLayout(const std::filesystem::path& projectRoot);
    static Result loadAndLayout(const std::filesystem::path& projectRoot,
                                const std::filesystem::path& designFile);
    static Result loadAndLayout(const std::filesystem::path& projectRoot,
                                const std::filesystem::path& designFile,
                                const std::string& moduleName);
    static Result refreshAndLoad(const std::filesystem::path& projectRoot,
                                 const std::filesystem::path& binDir,
                                 const CompilationFlow& flow,
                                 const std::string& projectName,
                                 const std::string& topModuleName,
                                 const std::string& topModuleFile,
                                 const std::string& mainTestFile,
                                 const std::string& additionalSources,
                                 const std::string& stage = "Compilation",
                                 const std::string& moduleName = {});
};
