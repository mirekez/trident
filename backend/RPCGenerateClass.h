#pragma once

#include <string>
#include <vector>

class RPCGenerateClass {
public:
    struct Field {
        std::string name;
        std::string type;
        int width = 32;
        int size = 4;
        bool packedArray = false;
        bool isRegister = false;
        bool combinational = false;
    };

    struct Request {
        std::string className;
        std::vector<Field> ports;
        std::vector<Field> members;
        bool generateTestHarness = false;
        bool generateVerilatorTestHarness = false;
        bool generateNegedgeWork = false;
    };

    struct Result {
        bool ok = false;
        std::string error;
        std::string content;
    };

    static Request emptyRequest();
    static Request sampleRequest();
    static Request parseRequest(const std::string& json);
    static Result generate(const Request& request);
    static std::string defaultsJson(bool testMode);
    static std::string requestToJson(const Request& request);
    static std::string jsonEscape(const std::string& value);
    static bool isValidCppIdentifier(const std::string& value);
    static bool isValidClassName(const std::string& value);
};
