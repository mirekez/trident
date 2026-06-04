#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::string quote(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

bool openBrowser(const std::string& url) {
#if defined(_WIN32)
    std::string command = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    std::string command = "open \"" + url + "\"";
#else
    const char* browsers[] = {
        "firefox",
        "google-chrome",
        "google-chrome-stable",
        "chromium-browser",
        "chromium",
        "brave-browser",
        "microsoft-edge"
    };

    for (const auto* browser : browsers) {
        const std::string command =
            "command -v " + std::string(browser) + " >/dev/null 2>&1 && " +
            std::string(browser) + " \"" + url + "\" >/dev/null 2>&1 &";
        if (std::system(command.c_str()) == 0) {
            return true;
        }
    }

    std::string command = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
    return std::system(command.c_str()) == 0;
}

} // namespace

int main(int argc, char** argv) {
    bool testMode = false;
    std::string port = "8080";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--test") {
            testMode = true;
        } else {
            port = arg;
        }
    }

    const auto exeDir = std::filesystem::absolute(argv[0]).parent_path();
#if defined(_WIN32)
    const auto backendExe = exeDir / "trident_backend.exe";
#else
    const auto backendExe = exeDir / "trident_backend";
#endif

    std::string command = quote(backendExe);
    if (testMode) {
        command += " --test";
    }
    command += " " + port;
    const std::string url = "http://127.0.0.1:" + port;

    std::thread([url]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        if (!openBrowser(url)) {
            std::cerr << "Backend is running, but failed to open browser. Open " << url << " manually.\n";
        }
    }).detach();

    std::cout << "Starting backend on " << url << "\n";
    if (testMode) {
        std::cout << "Using backend/test RPC implementations.\n";
    }
    const int rc = std::system(command.c_str());
    if (rc != 0) {
        std::cerr << "Backend process exited with code " << rc << ".\n";
        return 1;
    }

    return 0;
}
