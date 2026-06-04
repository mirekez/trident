#include "Server.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    bool testMode = false;
    unsigned short port = 8080;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--test") {
            testMode = true;
        } else {
            port = static_cast<unsigned short>(std::atoi(arg.c_str()));
        }
    }

    if (port == 0) {
        std::cerr << "Usage: trident_backend [--test] [port]\n";
        return 1;
    }

    Server server(port, testMode);
    return server.run();
}
