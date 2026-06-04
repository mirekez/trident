#include "RPCStatus.h"

#include "RPCStatusTest.h"

std::string RPCStatus::name() {
    return "status";
}

std::string RPCStatus::handle() {
    return RPCStatusTest::sampleJson();
}
