#include "Server.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    const auto port = static_cast<unsigned short>(argc > 1 ? std::atoi(argv[1]) : 8080);
    if (port == 0) {
        std::cerr << "Usage: trident_backend [port]\n";
        return 1;
    }

    Server server(port);
    return server.run();
}
