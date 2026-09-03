#include "RPCMetricsTest.h"
#include "RPCPictureTest.h"
#include "RPCStatusTest.h"
#include "RPCLoadFileTest.h"
#include "RPCGetOpenedFileListTest.h"
#include "RPCRefreshDevLogTest.h"
#include "RPCExecBashCommandTest.h"
#include "RPCRunBashCommandTest.h"
#include "RPCGenerateClassTest.h"
#include "RPCSchematicsTest.h"
#include "SchematicsDrilling.h"
#include "ProjectOneFileTest.h"

#include <iostream>

int main() {
    bool ok = true;
    ok = RPCStatusTest::validate() && ok;
    ok = RPCMetricsTest::validate() && ok;
    ok = RPCPictureTest::validate() && ok;
    ok = RPCLoadFileTest::validate() && ok;
    ok = RPCGetOpenedFileListTest::validate() && ok;
    ok = RPCRefreshDevLogTest::validate() && ok;
    ok = RPCExecBashCommandTest::validate() && ok;
    ok = RPCRunBashCommandTest::validate() && ok;
    ok = RPCGenerateClassTest::validate() && ok;
    ok = RPCSchematicsTest::validate() && ok;
    ok = SchematicsDrilling::validate() && ok;
    ok = ProjectOneFileTest::validate() && ok;

    if (!ok) {
        std::cerr << "One or more RPC test payloads failed validation.\n";
        return 1;
    }

    std::cout << "RPC test payloads validated.\n";
    return 0;
}
