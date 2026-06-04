#include "RPCMetricsTest.h"
#include "RPCPictureTest.h"
#include "RPCStatusTest.h"
#include "RPCLoadFileTest.h"

#include <iostream>

int main() {
    bool ok = true;
    ok = RPCStatusTest::validate() && ok;
    ok = RPCMetricsTest::validate() && ok;
    ok = RPCPictureTest::validate() && ok;
    ok = RPCLoadFileTest::validate() && ok;

    if (!ok) {
        std::cerr << "One or more RPC test payloads failed validation.\n";
        return 1;
    }

    std::cout << "RPC test payloads validated.\n";
    return 0;
}
