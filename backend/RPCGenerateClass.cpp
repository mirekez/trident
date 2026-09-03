#include "RPCGenerateClass.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace {

bool endsWith(const std::string& value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string jsonStringValue(const std::string& body, const std::string& key) {
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return {};
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return {};
    }
    const auto firstQuote = body.find('"', colon + 1);
    if (firstQuote == std::string::npos) {
        return {};
    }

    std::string value;
    for (std::size_t i = firstQuote + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (ch == '"') {
            return value;
        }
        if (ch != '\\' || i + 1 >= body.size()) {
            value += ch;
            continue;
        }
        const char escaped = body[++i];
        switch (escaped) {
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        case '\\': value += '\\'; break;
        case '"': value += '"'; break;
        default: value += escaped; break;
        }
    }
    return value;
}

bool jsonBoolValue(const std::string& body, const std::string& key) {
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return false;
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return false;
    }
    const auto valueStart = body.find_first_not_of(" \t\r\n", colon + 1);
    return valueStart != std::string::npos && body.compare(valueStart, 4, "true") == 0;
}

int jsonIntValue(const std::string& body, const std::string& key, int fallback) {
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return fallback;
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return fallback;
    }
    const auto valueStart = body.find_first_of("-0123456789", colon + 1);
    if (valueStart == std::string::npos) {
        return fallback;
    }
    char* end = nullptr;
    const long value = std::strtol(body.c_str() + valueStart, &end, 10);
    return end == body.c_str() + valueStart ? fallback : static_cast<int>(value);
}

std::vector<std::string> jsonObjectArray(const std::string& body, const std::string& key) {
    std::vector<std::string> objects;
    const auto keyPos = body.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return objects;
    }
    const auto colon = body.find(':', keyPos);
    if (colon == std::string::npos) {
        return objects;
    }
    const auto arrayStart = body.find('[', colon + 1);
    if (arrayStart == std::string::npos) {
        return objects;
    }

    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    std::size_t objectStart = std::string::npos;
    for (std::size_t i = arrayStart + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (inString && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (ch == '{') {
            if (objectDepth == 0) {
                objectStart = i;
            }
            ++objectDepth;
        } else if (ch == '}') {
            --objectDepth;
            if (objectDepth == 0 && objectStart != std::string::npos) {
                objects.push_back(body.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
            }
        } else if (ch == ']' && objectDepth == 0) {
            break;
        }
    }
    return objects;
}

bool validType(const std::string& type) {
    static const std::unordered_set<std::string> types = {
        "int", "uint", "logic", "struct", "int array", "uint array",
        "logic array", "struct array", "interface"
    };
    return types.contains(type);
}

bool hiddenPortName(const std::string& name) {
    return name == "clk_in" || name == "reset_in";
}

bool isArrayType(const std::string& type) {
    return type.find("array") != std::string::npos;
}

std::string intTypeForWidth(int width) {
    if (width <= 8) return "i8";
    if (width <= 16) return "i16";
    if (width <= 32) return "i32";
    return "i64";
}

std::string scalarTypeFor(const RPCGenerateClass::Field& field, const std::string& className) {
    const int width = std::clamp(field.width, 1, 64);
    if (field.type == "int" || field.type == "int array") {
        return intTypeForWidth(width);
    }
    if (field.type == "uint" || field.type == "uint array") {
        return "u<" + std::to_string(width) + ">";
    }
    if (field.type == "logic" || field.type == "logic array") {
        return "logic<" + std::to_string(width) + ">";
    }
    if (field.type == "struct" || field.type == "struct array") {
        return className + "Struct";
    }
    if (field.type == "interface") {
        return className + "Interface";
    }
    return "logic<32>";
}

std::string cppTypeFor(const RPCGenerateClass::Field& field, const std::string& className) {
    const std::string scalar = scalarTypeFor(field, className);
    if (!isArrayType(field.type)) {
        return scalar;
    }
    const int size = std::max(1, field.size);
    return "array<" + scalar + ", " + std::to_string(size) + ", " + (field.packedArray ? "true" : "false") + ">";
}

std::string fieldJson(const RPCGenerateClass::Field& field, bool member) {
    std::ostringstream json;
    json << "{\"name\":\"" << RPCGenerateClass::jsonEscape(field.name)
         << "\",\"type\":\"" << RPCGenerateClass::jsonEscape(field.type)
         << "\",\"width\":" << field.width
         << ",\"size\":" << field.size
         << ",\"packedArray\":" << (field.packedArray ? "true" : "false");
    if (member) {
        json << ",\"isRegister\":" << (field.isRegister ? "true" : "false")
             << ",\"combinational\":" << (field.combinational ? "true" : "false");
    }
    json << "}";
    return json.str();
}

std::string validationError(const RPCGenerateClass::Request& request) {
    if (!RPCGenerateClass::isValidClassName(request.className)) {
        return "invalid_class_name";
    }
    for (const auto& port : request.ports) {
        if (!RPCGenerateClass::isValidCppIdentifier(port.name)) {
            return "invalid_port_name";
        }
        if (hiddenPortName(port.name)) {
            return "hidden_port_name";
        }
        if (!endsWith(port.name, "_in") && !endsWith(port.name, "_out")) {
            return "port_suffix_required";
        }
        if (!validType(port.type)) {
            return "invalid_port_type";
        }
        if (port.width <= 0 || port.size <= 0) {
            return "invalid_port_size";
        }
    }
    for (const auto& member : request.members) {
        if (!RPCGenerateClass::isValidCppIdentifier(member.name)) {
            return "invalid_member_name";
        }
        if (!validType(member.type)) {
            return "invalid_member_type";
        }
        if (member.width <= 0 || member.size <= 0) {
            return "invalid_member_size";
        }
        if (member.isRegister && member.combinational) {
            return "member_register_and_comb";
        }
        if (member.combinational && !endsWith(member.name, "_comb")) {
            return "member_comb_suffix_required";
        }
    }
    return {};
}

void writePlaceholderTypes(std::ostringstream& out, const std::string& className, bool needStruct, bool needInterface) {
    if (needStruct) {
        out << "struct " << className << "Struct\n"
            << "{\n"
            << "    logic<32> data = 0;\n\n"
            << "    constexpr static size_t _size_bits() { return 32; }\n"
            << "    logic<32> pack() const { return data; }\n"
            << "    " << className << "Struct& operator=(const logic<32>& value)\n"
            << "    {\n"
            << "        data = value;\n"
            << "        return *this;\n"
            << "    }\n"
            << "};\n\n";
    }
    if (needInterface) {
        out << "class " << className << "Interface : public Interface\n"
            << "{\n"
            << "public:\n"
            << "    logic<32> payload = 0;\n\n"
            << "    constexpr static size_t _size_bits() { return 32; }\n"
            << "    logic<32> pack() const { return payload; }\n"
            << "    " << className << "Interface& operator=(const logic<32>& value)\n"
            << "    {\n"
            << "        payload = value;\n"
            << "        return *this;\n"
            << "    }\n"
            << "    void _assign() {}\n"
            << "};\n\n";
    }
}

} // namespace

RPCGenerateClass::Request RPCGenerateClass::emptyRequest() {
    Request request;
    request.className = "NewModule";
    request.ports = {
        {"valid_in", "logic", 1, 1, false, false, false},
        {"ready_out", "logic", 1, 1, false, false, false}
    };
    request.members = {
        {"state_reg", "uint", 8, 1, false, true, false},
        {"ready_comb", "logic", 1, 1, false, false, true}
    };
    return request;
}

RPCGenerateClass::Request RPCGenerateClass::sampleRequest() {
    Request request;
    request.className = "GeneratedDemo";
    request.ports = {
        {"signed_value_in", "int", 32, 1, false, false, false},
        {"unsigned_value_in", "uint", 16, 1, false, false, false},
        {"logic_value_in", "logic", 12, 1, false, false, false},
        {"struct_value_in", "struct", 32, 1, false, false, false},
        {"int_items_in", "int array", 32, 3, false, false, false},
        {"uint_items_in", "uint array", 8, 4, true, false, false},
        {"logic_items_out", "logic array", 7, 5, true, false, false},
        {"struct_items_out", "struct array", 32, 2, false, false, false},
        {"bus_if_in", "interface", 32, 1, false, false, false}
    };
    request.members = {
        {"signed_state", "int", 32, 1, false, false, false},
        {"unsigned_count_reg", "uint", 16, 1, false, true, false},
        {"logic_state", "logic", 12, 1, false, false, false},
        {"packet_struct", "struct", 32, 1, false, false, false},
        {"int_buffer", "int array", 32, 3, false, false, false},
        {"uint_buffer_reg", "uint array", 8, 4, true, true, false},
        {"logic_buffer_comb", "logic array", 7, 5, true, false, true},
        {"struct_buffer", "struct array", 32, 2, false, false, false},
        {"local_if", "interface", 32, 1, false, false, false}
    };
    request.generateTestHarness = true;
    request.generateVerilatorTestHarness = true;
    request.generateNegedgeWork = true;
    return request;
}

RPCGenerateClass::Request RPCGenerateClass::parseRequest(const std::string& json) {
    Request request;
    request.className = jsonStringValue(json, "className");
    request.generateTestHarness = jsonBoolValue(json, "generateTestHarness");
    request.generateVerilatorTestHarness = jsonBoolValue(json, "generateVerilatorTestHarness");
    request.generateNegedgeWork = jsonBoolValue(json, "generateNegedgeWork");

    const auto parseField = [](const std::string& object, bool member) {
        Field field;
        field.name = jsonStringValue(object, "name");
        field.type = jsonStringValue(object, "type");
        field.width = jsonIntValue(object, "width", 32);
        field.size = jsonIntValue(object, "size", 4);
        field.packedArray = jsonBoolValue(object, "packedArray");
        if (member) {
            field.isRegister = jsonBoolValue(object, "isRegister");
            field.combinational = jsonBoolValue(object, "combinational");
        }
        return field;
    };

    for (const auto& object : jsonObjectArray(json, "ports")) {
        request.ports.push_back(parseField(object, false));
    }
    for (const auto& object : jsonObjectArray(json, "members")) {
        request.members.push_back(parseField(object, true));
    }
    return request;
}

RPCGenerateClass::Result RPCGenerateClass::generate(const Request& request) {
    if (const auto error = validationError(request); !error.empty()) {
        return {false, error, {}};
    }

    bool needStruct = false;
    bool needInterface = false;
    const auto markTypes = [&](const std::vector<Field>& fields) {
        for (const auto& field : fields) {
            needStruct = needStruct || field.type == "struct" || field.type == "struct array";
            needInterface = needInterface || field.type == "interface";
        }
    };
    markTypes(request.ports);
    markTypes(request.members);

    std::ostringstream out;
    out << "#pragma once\n\n"
        << "#include \"cpphdl.h\"\n\n"
        << "using namespace cpphdl;\n\n"
        << "extern long _system_clock;\n\n";

    if (request.generateVerilatorTestHarness) {
        out << "#if defined(VERILATOR)\n"
            << "#define TRIDENT_STRINGIFY_IMPL(name) #name\n"
            << "#define TRIDENT_STRINGIFY(name) TRIDENT_STRINGIFY_IMPL(name)\n"
            << "#define TRIDENT_VERILATOR_HEADER(name) TRIDENT_STRINGIFY(name.h)\n"
            << "#include TRIDENT_VERILATOR_HEADER(VERILATOR_MODEL)\n"
            << "#endif\n\n";
    }

    writePlaceholderTypes(out, request.className, needStruct, needInterface);

    out << "class " << request.className << " : public Module\n"
        << "{\n"
        << "public:\n";

    for (const auto& port : request.ports) {
        const auto type = cppTypeFor(port, request.className);
        out << "    _PORT(" << type << ") " << port.name;
        if (endsWith(port.name, "_out")) {
            out << " = _ASSIGN_COMB(" << port.name << "_value)";
        }
        out << ";\n";
    }

    out << "\nprivate:\n";
    for (const auto& port : request.ports) {
        if (endsWith(port.name, "_out")) {
            out << "    " << cppTypeFor(port, request.className) << " " << port.name << "_value{};\n";
        }
    }
    for (const auto& member : request.members) {
        const auto type = cppTypeFor(member, request.className);
        if (member.isRegister) {
            out << "    reg<" << type << "> " << member.name << ";\n";
        } else if (member.combinational) {
            out << "    _LAZY_COMB(" << member.name << ", " << type << ")\n"
                << "        " << member.name << " = {};\n"
                << "        return " << member.name << ";\n"
                << "    }\n";
        } else {
            out << "    " << type << " " << member.name << "{};\n";
        }
    }

    out << "\npublic:\n"
        << "    void _work(bool reset)\n"
        << "    {\n"
        << "        (void)reset;\n";
    for (const auto& member : request.members) {
        if (member.isRegister) {
            out << "        " << member.name << "._next = " << member.name << ";\n";
        }
    }
    out << "    }\n\n";

    if (request.generateNegedgeWork) {
        out << "    void _work_neg(bool reset)\n"
            << "    {\n"
            << "        (void)reset;\n"
            << "    }\n\n";
    }

    out << "    void _strobe()\n"
        << "    {\n";
    for (const auto& member : request.members) {
        if (member.isRegister) {
            out << "        " << member.name << ".strobe();\n";
        }
    }
    out << "    }\n\n"
        << "    void _assign() {}\n"
        << "};\n";

    if (request.generateTestHarness || request.generateVerilatorTestHarness) {
        out << "\n#if !defined(SYNTHESIS)\n"
            << "class " << request.className << "TestHarness : public Module\n"
            << "{\n"
            << "public:\n";
        if (request.generateVerilatorTestHarness) {
            out << "#if defined(VERILATOR)\n"
                << "    VERILATOR_MODEL dut;\n"
                << "#else\n"
                << "    " << request.className << " dut;\n"
                << "#endif\n";
        } else {
            out << "    " << request.className << " dut;\n";
        }
        out << "\n"
            << "    void _work(bool reset)\n"
            << "    {\n";
        if (request.generateVerilatorTestHarness) {
            out << "#if defined(VERILATOR)\n"
                << "        dut.clk = 0;\n"
                << "        dut.reset = reset ? 1 : 0;\n"
                << "        dut.eval();\n"
                << "#else\n"
                << "        dut._assign();\n"
                << "        dut._work(reset);\n"
                << "#endif\n";
        } else {
            out << "        dut._assign();\n"
                << "        dut._work(reset);\n";
        }
        out << "    }\n\n";
        if (request.generateNegedgeWork || request.generateVerilatorTestHarness) {
            out << "    void _work_neg(bool reset)\n"
                << "    {\n";
            if (request.generateVerilatorTestHarness) {
                out << "#if defined(VERILATOR)\n"
                    << "        dut.clk = 1;\n"
                    << "        dut.reset = reset ? 1 : 0;\n"
                    << "        dut.eval();\n"
                    << "#else\n"
                    << "        dut._work_neg(reset);\n"
                    << "#endif\n";
            } else {
                out << "        dut._work_neg(reset);\n";
            }
            out << "    }\n\n";
        }
        out << "    void _strobe_neg()\n"
            << "    {\n";
        if (request.generateVerilatorTestHarness) {
            out << "#if !defined(VERILATOR)\n"
                << "        dut._strobe_neg();\n"
                << "#endif\n";
        } else {
            out << "        dut._strobe_neg();\n";
        }
        out << "    }\n\n"
            << "    void _strobe()\n"
            << "    {\n";
        if (request.generateVerilatorTestHarness) {
            out << "#if !defined(VERILATOR)\n"
                << "        dut._strobe();\n"
                << "#endif\n";
        } else {
            out << "        dut._strobe();\n";
        }
        out << "    }\n\n"
            << "    void _assign() {}\n"
            << "};\n\n"
            << "#if defined(TRIDENT_CLASS_GENERATOR_STANDALONE)\n"
            << "long _system_clock = -1;\n"
            << "int main()\n"
            << "{\n"
            << "    " << request.className << "TestHarness harness;\n"
            << "    harness._assign();\n"
            << "    harness._work(true);\n"
            << "    harness._work_neg(true);\n"
            << "    harness._strobe();\n"
            << "    ++_system_clock;\n"
            << "    harness._work(false);\n"
            << "    harness._work_neg(false);\n"
            << "    harness._strobe();\n"
            << "    return 0;\n"
            << "}\n"
            << "#endif\n"
            << "#endif\n";
    }

    return {true, {}, out.str()};
}

std::string RPCGenerateClass::defaultsJson(bool testMode) {
    return requestToJson(testMode ? sampleRequest() : emptyRequest());
}

std::string RPCGenerateClass::requestToJson(const Request& request) {
    std::ostringstream json;
    json << "{\"className\":\"" << jsonEscape(request.className) << "\",\"ports\":[";
    for (std::size_t i = 0; i < request.ports.size(); ++i) {
        if (i != 0) json << ',';
        json << fieldJson(request.ports[i], false);
    }
    json << "],\"members\":[";
    for (std::size_t i = 0; i < request.members.size(); ++i) {
        if (i != 0) json << ',';
        json << fieldJson(request.members[i], true);
    }
    json << "],\"generateTestHarness\":" << (request.generateTestHarness ? "true" : "false")
         << ",\"generateVerilatorTestHarness\":" << (request.generateVerilatorTestHarness ? "true" : "false")
         << ",\"generateNegedgeWork\":" << (request.generateNegedgeWork ? "true" : "false")
         << "}";
    return json.str();
}

std::string RPCGenerateClass::jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
}

bool RPCGenerateClass::isValidCppIdentifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const auto first = static_cast<unsigned char>(value.front());
    if (!(std::isalpha(first) || value.front() == '_')) {
        return false;
    }
    for (const char ch : value) {
        const auto uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '_')) {
            return false;
        }
    }
    static const std::unordered_set<std::string> keywords = {
        "alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch",
        "char", "class", "const", "constexpr", "continue", "decltype", "default", "delete",
        "do", "double", "else", "enum", "explicit", "export", "extern", "false", "float",
        "for", "friend", "goto", "if", "inline", "int", "long", "namespace", "new",
        "noexcept", "not", "operator", "private", "protected", "public", "register",
        "return", "short", "signed", "sizeof", "static", "struct", "switch", "template",
        "this", "throw", "true", "try", "typedef", "typename", "union", "unsigned",
        "using", "virtual", "void", "volatile", "while"
    };
    return !keywords.contains(value);
}

bool RPCGenerateClass::isValidClassName(const std::string& value) {
    return isValidCppIdentifier(value) && !value.starts_with('_');
}
