#include "RPCSchematics.h"

#include "CompilationFlow.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    std::optional<JsonValue> parse() {
        skipWhitespace();
        auto value = parseValue();
        skipWhitespace();
        if (!value || pos_ != text_.size()) {
            return std::nullopt;
        }
        return value;
    }

private:
    std::optional<JsonValue> parseValue() {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            return std::nullopt;
        }
        const char ch = text_[pos_];
        if (ch == '"') {
            return parseString();
        }
        if (ch == '{') {
            return parseObject();
        }
        if (ch == '[') {
            return parseArray();
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return parseNumber();
        }
        if (consumeLiteral("true")) {
            JsonValue value;
            value.type = JsonValue::Type::Bool;
            value.boolValue = true;
            return value;
        }
        if (consumeLiteral("false")) {
            JsonValue value;
            value.type = JsonValue::Type::Bool;
            return value;
        }
        if (consumeLiteral("null")) {
            return JsonValue{};
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parseObject() {
        JsonValue value;
        value.type = JsonValue::Type::Object;
        ++pos_;
        skipWhitespace();
        if (consume('}')) {
            return value;
        }
        while (pos_ < text_.size()) {
            auto key = parseString();
            if (!key || key->type != JsonValue::Type::String) {
                return std::nullopt;
            }
            skipWhitespace();
            if (!consume(':')) {
                return std::nullopt;
            }
            auto entry = parseValue();
            if (!entry) {
                return std::nullopt;
            }
            value.objectValue.emplace(key->stringValue, std::move(*entry));
            skipWhitespace();
            if (consume('}')) {
                return value;
            }
            if (!consume(',')) {
                return std::nullopt;
            }
            skipWhitespace();
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parseArray() {
        JsonValue value;
        value.type = JsonValue::Type::Array;
        ++pos_;
        skipWhitespace();
        if (consume(']')) {
            return value;
        }
        while (pos_ < text_.size()) {
            auto entry = parseValue();
            if (!entry) {
                return std::nullopt;
            }
            value.arrayValue.push_back(std::move(*entry));
            skipWhitespace();
            if (consume(']')) {
                return value;
            }
            if (!consume(',')) {
                return std::nullopt;
            }
            skipWhitespace();
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parseString() {
        if (!consume('"')) {
            return std::nullopt;
        }
        JsonValue value;
        value.type = JsonValue::Type::String;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return value;
            }
            if (ch != '\\') {
                value.stringValue += ch;
                continue;
            }
            if (pos_ >= text_.size()) {
                return std::nullopt;
            }
            const char escaped = text_[pos_++];
            switch (escaped) {
            case '"': value.stringValue += '"'; break;
            case '\\': value.stringValue += '\\'; break;
            case '/': value.stringValue += '/'; break;
            case 'b': value.stringValue += '\b'; break;
            case 'f': value.stringValue += '\f'; break;
            case 'n': value.stringValue += '\n'; break;
            case 'r': value.stringValue += '\r'; break;
            case 't': value.stringValue += '\t'; break;
            case 'u':
                if (pos_ + 4 > text_.size()) {
                    return std::nullopt;
                }
                value.stringValue += '?';
                pos_ += 4;
                break;
            default:
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parseNumber() {
        const auto start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }

        JsonValue value;
        value.type = JsonValue::Type::Number;
        try {
            value.numberValue = std::stod(text_.substr(start, pos_ - start));
        } catch (...) {
            return std::nullopt;
        }
        return value;
    }

    bool consume(char expected) {
        skipWhitespace();
        if (pos_ >= text_.size() || text_[pos_] != expected) {
            return false;
        }
        ++pos_;
        return true;
    }

    bool consumeLiteral(const char* literal) {
        const std::string value(literal);
        if (text_.compare(pos_, value.size(), value) != 0) {
            return false;
        }
        pos_ += value.size();
        return true;
    }

    void skipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    std::string text_;
    std::size_t pos_ = 0;
};

struct Port {
    std::string name;
    std::string direction = "input";
    std::string width;
    int x = 0;
    int y = 0;
};

struct Module {
    std::string id;
    std::string title;
    std::string moduleName;
    std::vector<Port> ports;
    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    int column = 0;
    int index = 0;
    int x = 0;
    int y = 0;
    int width = 190;
    int height = 90;
};

struct Endpoint {
    std::string module;
    std::string port;
};

struct Connection {
    std::string net;
    Endpoint from;
    std::vector<Endpoint> to;
};

struct NetEndpoint {
    Endpoint endpoint;
    std::vector<int> bits;
    std::string direction;
};

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
}

const JsonValue* objectMember(const JsonValue& value, const std::string& key) {
    if (value.type != JsonValue::Type::Object) {
        return nullptr;
    }
    const auto it = value.objectValue.find(key);
    return it == value.objectValue.end() ? nullptr : &it->second;
}

std::string stringMember(const JsonValue& value, const std::string& key, const std::string& fallback = {}) {
    const auto* member = objectMember(value, key);
    if (!member || member->type != JsonValue::Type::String) {
        return fallback;
    }
    return member->stringValue;
}

std::string valueAsString(const JsonValue* value) {
    if (!value) {
        return {};
    }
    if (value->type == JsonValue::Type::String) {
        return value->stringValue;
    }
    if (value->type == JsonValue::Type::Number) {
        const auto rounded = static_cast<long long>(std::llround(value->numberValue));
        if (std::abs(value->numberValue - static_cast<double>(rounded)) < 0.000001) {
            return std::to_string(rounded);
        }
        std::ostringstream out;
        out << value->numberValue;
        return out.str();
    }
    return {};
}

std::vector<int> intArrayValue(const JsonValue* value) {
    std::vector<int> out;
    if (!value || value->type != JsonValue::Type::Array) {
        return out;
    }
    for (const auto& entry : value->arrayValue) {
        if (entry.type == JsonValue::Type::Number) {
            out.push_back(static_cast<int>(std::llround(entry.numberValue)));
        }
    }
    return out;
}

bool truthyValue(const JsonValue* value) {
    if (!value) {
        return false;
    }
    if (value->type == JsonValue::Type::Bool) {
        return value->boolValue;
    }
    if (value->type == JsonValue::Type::Number) {
        return std::llround(value->numberValue) != 0;
    }
    if (value->type == JsonValue::Type::String) {
        return value->stringValue == "1" || value->stringValue == "true";
    }
    return false;
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
    const auto valueStart = body.find_first_not_of(" \t\r\n", colon + 1);
    if (valueStart == std::string::npos) {
        return fallback;
    }
    char* end = nullptr;
    const auto parsed = std::strtol(body.c_str() + valueStart, &end, 10);
    if (end == body.c_str() + valueStart) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

std::string bitKey(const std::vector<int>& bits) {
    std::ostringstream key;
    for (std::size_t i = 0; i < bits.size(); ++i) {
        if (i != 0) {
            key << ',';
        }
        key << bits[i];
    }
    return key.str();
}

bool bitVectorsIntersect(const std::vector<int>& lhs, const std::vector<int>& rhs) {
    for (const int a : lhs) {
        for (const int b : rhs) {
            if (a == b) {
                return true;
            }
        }
    }
    return false;
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string cpphdlPortStem(const std::string& name) {
    if (endsWith(name, "_out")) {
        return name.substr(0, name.size() - 4);
    }
    if (endsWith(name, "_in")) {
        return name.substr(0, name.size() - 3);
    }
    return {};
}

bool cpphdlPortsLookConnected(const NetEndpoint& driver, const NetEndpoint& sink) {
    if (driver.bits.empty() || sink.bits.empty() || driver.bits.size() != sink.bits.size()) {
        return false;
    }
    if (!endsWith(driver.endpoint.port, "_out") || !endsWith(sink.endpoint.port, "_in")) {
        return false;
    }
    const auto driverStem = cpphdlPortStem(driver.endpoint.port);
    return !driverStem.empty() && driverStem == cpphdlPortStem(sink.endpoint.port);
}

std::string cleanId(std::string value) {
    for (char& ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }
    return value;
}

Endpoint endpointFromJson(const JsonValue& value) {
    return {
        cleanId(stringMember(value, "module", stringMember(value, "moduleId"))),
        stringMember(value, "port")
    };
}

void addPort(Module& module, Port port) {
    const auto portIndex = module.ports.size();
    if (port.direction == "output") {
        module.outputs.push_back(portIndex);
    } else {
        module.inputs.push_back(portIndex);
    }
    module.ports.push_back(std::move(port));
}

const JsonValue* chooseYosysTopModule(const JsonValue& moduleObject, const std::string& requestedName, std::string& topName) {
    if (!requestedName.empty()) {
        if (const auto requested = moduleObject.objectValue.find(requestedName); requested != moduleObject.objectValue.end() &&
                requested->second.type == JsonValue::Type::Object) {
            topName = requested->first;
            return &requested->second;
        }
        return nullptr;
    }

    const JsonValue* fallback = nullptr;
    std::size_t fallbackCellCount = 0;

    for (const auto& [moduleName, moduleJson] : moduleObject.objectValue) {
        if (moduleJson.type != JsonValue::Type::Object) {
            continue;
        }
        const auto* attributes = objectMember(moduleJson, "attributes");
        if (attributes) {
            const auto* topAttribute = objectMember(*attributes, "top");
            const auto topText = valueAsString(topAttribute);
            if (topText == "1" || topText == "true") {
                topName = moduleName;
                return &moduleJson;
            }
        }

        const auto* cells = objectMember(moduleJson, "cells");
        const std::size_t cellCount = cells && cells->type == JsonValue::Type::Object ? cells->objectValue.size() : 0;
        if (!fallback || cellCount > fallbackCellCount) {
            fallback = &moduleJson;
            fallbackCellCount = cellCount;
            topName = moduleName;
        }
    }
    return fallback;
}

std::string netLabelForBits(const std::map<std::string, std::string>& vectorNames,
                            const std::map<int, std::string>& bitNames,
                            const std::vector<int>& bits) {
    if (const auto exact = vectorNames.find(bitKey(bits)); exact != vectorNames.end()) {
        return exact->second;
    }
    for (const int bit : bits) {
        if (const auto name = bitNames.find(bit); name != bitNames.end()) {
            return name->second;
        }
    }
    return "net_" + bitKey(bits);
}

int netNamePriority(const std::string& name) {
    if (endsWith(name, "_reg") || endsWith(name, "_value")) {
        return 3;
    }
    return name.find("__") == std::string::npos ? 2 : 1;
}

bool isYosysInternalCell(const std::string& cellName, const JsonValue& cellJson) {
    const auto type = stringMember(cellJson, "type", cellName);
    if (!type.empty() && type.front() == '$') {
        return true;
    }
    if (!cellName.empty() && cellName.front() == '$') {
        return true;
    }
    return truthyValue(objectMember(cellJson, "hide_name"));
}

void appendCpphdlDataMembers(const JsonValue& moduleJson, std::vector<Module>& modules) {
    const auto* members = objectMember(moduleJson, "data_members");
    if (!members || members->type != JsonValue::Type::Object) {
        return;
    }
    for (const auto& [memberName, memberJson] : members->objectValue) {
        Module module;
        module.id = cleanId("__member__" + memberName);
        const auto kind = stringMember(memberJson, "kind", "signal");
        const auto width = valueAsString(objectMember(memberJson, "width"));
        module.title = memberName + " : " + kind;
        if (!width.empty()) {
            module.title += " [" + width + "]";
        }
        modules.push_back(std::move(module));
    }
}

bool parseYosysDesign(const JsonValue& root, const std::string& requestedName, bool showTopBoundaryAsModule,
                      std::string& name, std::vector<Module>& modules,
                      std::vector<Connection>& connections) {
    const auto* moduleObject = objectMember(root, "modules");
    if (!moduleObject || moduleObject->type != JsonValue::Type::Object) {
        return false;
    }

    std::string topName;
    const auto* top = chooseYosysTopModule(*moduleObject, requestedName, topName);
    if (!top) {
        return false;
    }
    name = topName.empty() ? stringMember(root, "name", "RTL Design") : topName;
    const bool allowCpphdlPortNameLinks = stringMember(root, "creator") == "cpphdl";

    const auto* topPorts = objectMember(*top, "ports");
    const auto* topCells = objectMember(*top, "cells");
    const auto* topDataMembers = objectMember(*top, "data_members");
    const bool hasDataMembers = topDataMembers && topDataMembers->type == JsonValue::Type::Object &&
        !topDataMembers->objectValue.empty();
    bool hasUserCells = false;
    if (topCells && topCells->type == JsonValue::Type::Object) {
        for (const auto& [cellName, cellJson] : topCells->objectValue) {
            if (!isYosysInternalCell(cellName, cellJson)) {
                hasUserCells = true;
                break;
            }
        }
    }
    if (topPorts && topPorts->type == JsonValue::Type::Object &&
            (!topCells || topCells->type != JsonValue::Type::Object || !hasUserCells)) {
        Module topModule;
        topModule.id = cleanId(name);
        topModule.title = name;
        topModule.moduleName = requestedName.empty() && hasDataMembers ? name : std::string{};
        for (const auto& [portName, portJson] : topPorts->objectValue) {
            const auto bits = intArrayValue(objectMember(portJson, "bits"));
            if (bits.empty()) {
                continue;
            }
            Port port;
            port.name = portName;
            port.direction = stringMember(portJson, "direction", "input") == "output" ? "output" : "input";
            port.width = std::to_string(bits.size());
            addPort(topModule, std::move(port));
        }
        if (!topModule.ports.empty() || hasDataMembers) {
            modules.push_back(std::move(topModule));
            if (!requestedName.empty()) {
                appendCpphdlDataMembers(*top, modules);
            }
            return true;
        }
    }

    std::map<std::string, std::string> vectorNames;
    std::map<int, std::string> bitNames;
    if (const auto* netnames = objectMember(*top, "netnames"); netnames && netnames->type == JsonValue::Type::Object) {
        for (const auto& [netName, netJson] : netnames->objectValue) {
            const auto bits = intArrayValue(objectMember(netJson, "bits"));
            if (bits.empty()) {
                continue;
            }
            const auto key = bitKey(bits);
            const auto existingVector = vectorNames.find(key);
            if (existingVector == vectorNames.end() ||
                    netNamePriority(netName) > netNamePriority(existingVector->second)) {
                vectorNames[key] = netName;
            }
            for (const int bit : bits) {
                const auto existingBit = bitNames.find(bit);
                if (existingBit == bitNames.end() ||
                        netNamePriority(netName) > netNamePriority(existingBit->second)) {
                    bitNames[bit] = netName;
                }
            }
        }
    }

    std::vector<NetEndpoint> drivers;
    std::vector<NetEndpoint> sinks;
    std::vector<std::pair<Module, NetEndpoint>> topOutputModules;
    std::optional<Module> topBoundaryModule;
    std::vector<Port> topBoundaryExternalPorts;
    if (showTopBoundaryAsModule && requestedName.empty()) {
        Module module;
        module.id = cleanId("__top__" + topName);
        module.title = topName;
        module.moduleName = {};
        topBoundaryModule = std::move(module);
    }

    if (const auto* ports = objectMember(*top, "ports"); ports && ports->type == JsonValue::Type::Object) {
        for (const auto& [portName, portJson] : ports->objectValue) {
            const auto direction = stringMember(portJson, "direction", "input");
            const auto bits = intArrayValue(objectMember(portJson, "bits"));
            if (bits.empty()) {
                continue;
            }

            Port port;
            port.name = portName;
            port.width = std::to_string(bits.size());
            if (topBoundaryModule) {
                port.direction = direction == "output" ? "output" : "input";
                topBoundaryExternalPorts.push_back(std::move(port));
            } else {
                port.direction = direction == "output" ? "input" : "output";
                Module module;
                module.id = cleanId((direction == "output" ? "__top_out__" : "__top_in__") + portName);
                module.title = direction == "output" ? "output" : "input";
                module.moduleName = {};
                addPort(module, std::move(port));

                NetEndpoint endpoint{{module.id, portName}, bits, direction};
                if (direction == "output") {
                    topOutputModules.push_back({std::move(module), std::move(endpoint)});
                } else {
                    drivers.push_back(endpoint);
                    modules.push_back(std::move(module));
                }
            }
        }
    }

    if (const auto* cells = objectMember(*top, "cells"); cells && cells->type == JsonValue::Type::Object) {
        for (const auto& [cellName, cellJson] : cells->objectValue) {
            if (isYosysInternalCell(cellName, cellJson)) {
                continue;
            }
            Module module;
            module.id = cleanId(cellName);
            module.title = stringMember(cellJson, "type", cellName);
            module.moduleName = module.title;
            const auto* directions = objectMember(cellJson, "port_directions");
            const auto* cellConnections = objectMember(cellJson, "connections");
            if (!cellConnections || cellConnections->type != JsonValue::Type::Object) {
                continue;
            }

            for (const auto& [portName, bitsJson] : cellConnections->objectValue) {
                const auto bits = intArrayValue(&bitsJson);
                if (bits.empty()) {
                    continue;
                }
                std::string direction = "input";
                if (directions) {
                    direction = stringMember(*directions, portName, "input");
                }

                Port port;
                port.name = portName;
                port.direction = direction == "output" ? "output" : "input";
                port.width = std::to_string(bits.size());
                addPort(module, std::move(port));

                NetEndpoint endpoint{{module.id, portName}, bits, direction};
                if (direction == "output") {
                    drivers.push_back(endpoint);
                } else {
                    sinks.push_back(endpoint);
                }
            }

            modules.push_back(std::move(module));
        }
    }

    if (topBoundaryModule) {
        const auto boundaryId = topBoundaryModule->id;
        auto endpointsConnect = [&](const NetEndpoint& driver, const NetEndpoint& sink) {
            if (driver.endpoint.module == sink.endpoint.module) {
                return false;
            }
            if (bitVectorsIntersect(driver.bits, sink.bits)) {
                return true;
            }
            return allowCpphdlPortNameLinks && cpphdlPortsLookConnected(driver, sink);
        };
        auto uniquePortName = [&](std::string base) {
            if (base.empty()) {
                base = "signal";
            }
            const auto isUsed = [&](const std::string& candidate) {
                return std::any_of(topBoundaryModule->ports.begin(), topBoundaryModule->ports.end(),
                    [&](const Port& port) { return port.name == candidate; });
            };
            if (!isUsed(base)) {
                return base;
            }
            for (int suffix = 2; ; ++suffix) {
                const auto candidate = base + "_" + std::to_string(suffix);
                if (!isUsed(candidate)) {
                    return candidate;
                }
            }
        };

        const auto originalSinks = sinks;
        const auto originalDrivers = drivers;
        std::set<std::string> addedDriverBits;
        std::set<std::string> addedSinkBits;

        for (const auto& sink : originalSinks) {
            if (sink.endpoint.module == boundaryId ||
                    std::any_of(originalDrivers.begin(), originalDrivers.end(),
                        [&](const NetEndpoint& driver) { return endpointsConnect(driver, sink); })) {
                continue;
            }
            if (!addedDriverBits.insert(bitKey(sink.bits)).second) {
                continue;
            }
            const auto portName = uniquePortName(netLabelForBits(vectorNames, bitNames, sink.bits));
            addPort(*topBoundaryModule, {portName, "output", std::to_string(sink.bits.size())});
            drivers.push_back({{boundaryId, portName}, sink.bits, "output"});
        }

        for (const auto& driver : originalDrivers) {
            if (driver.endpoint.module == boundaryId ||
                    std::any_of(originalSinks.begin(), originalSinks.end(),
                        [&](const NetEndpoint& sink) { return endpointsConnect(driver, sink); })) {
                continue;
            }
            if (!addedSinkBits.insert(bitKey(driver.bits)).second) {
                continue;
            }
            const auto portName = uniquePortName(netLabelForBits(vectorNames, bitNames, driver.bits));
            addPort(*topBoundaryModule, {portName, "input", std::to_string(driver.bits.size())});
            sinks.push_back({{boundaryId, portName}, driver.bits, "input"});
        }

        for (auto& port : topBoundaryExternalPorts) {
            port.name = uniquePortName(port.name);
            addPort(*topBoundaryModule, std::move(port));
        }

        if (!topBoundaryModule->ports.empty()) {
            modules.insert(modules.begin(), std::move(*topBoundaryModule));
        }
    }

    for (auto& [module, endpoint] : topOutputModules) {
        sinks.push_back(endpoint);
        modules.push_back(std::move(module));
    }

    std::set<std::string> emitted;
    for (const auto& driver : drivers) {
        for (const auto& sink : sinks) {
            if (driver.endpoint.module == sink.endpoint.module) {
                continue;
            }
            const bool bitConnected = bitVectorsIntersect(driver.bits, sink.bits);
            const bool cpphdlNameConnected = allowCpphdlPortNameLinks && !bitConnected &&
                cpphdlPortsLookConnected(driver, sink);
            if (!bitConnected && !cpphdlNameConnected) {
                continue;
            }
            const std::string key = driver.endpoint.module + "." + driver.endpoint.port + ">" +
                sink.endpoint.module + "." + sink.endpoint.port + ":" + bitKey(driver.bits) + ":" + bitKey(sink.bits);
            if (!emitted.insert(key).second) {
                continue;
            }
            Connection connection;
            connection.net = bitConnected ? netLabelForBits(vectorNames, bitNames, driver.bits)
                                          : cpphdlPortStem(driver.endpoint.port);
            connection.from = driver.endpoint;
            connection.to.push_back(sink.endpoint);
            connections.push_back(std::move(connection));
        }
    }

    return !modules.empty();
}

bool parseDesign(const JsonValue& root, const std::string& requestedName, bool showTopBoundaryAsModule,
                 std::string& name, std::vector<Module>& modules,
                 std::vector<Connection>& connections) {
    if (root.type != JsonValue::Type::Object) {
        return false;
    }
    name = stringMember(root, "name", "RTL Design");

    const auto* moduleArray = objectMember(root, "modules");
    if (moduleArray && moduleArray->type == JsonValue::Type::Object) {
        return parseYosysDesign(root, requestedName, showTopBoundaryAsModule, name, modules, connections);
    }
    if (!moduleArray || moduleArray->type != JsonValue::Type::Array) {
        return false;
    }

    for (std::size_t i = 0; i < moduleArray->arrayValue.size(); ++i) {
        const auto& moduleJson = moduleArray->arrayValue[i];
        Module module;
        module.index = static_cast<int>(i);
        module.id = cleanId(stringMember(moduleJson, "id", stringMember(moduleJson, "name", "module_" + std::to_string(i + 1))));
        module.title = stringMember(moduleJson, "title", stringMember(moduleJson, "name", module.id));
        module.moduleName = stringMember(moduleJson, "moduleName", module.title);

        const auto* portArray = objectMember(moduleJson, "ports");
        if (portArray && portArray->type == JsonValue::Type::Array) {
            for (const auto& portJson : portArray->arrayValue) {
                Port port;
                port.name = stringMember(portJson, "name");
                port.direction = stringMember(portJson, "direction", "input") == "output" ? "output" : "input";
                port.width = valueAsString(objectMember(portJson, "width"));
                if (port.name.empty()) {
                    continue;
                }
                addPort(module, std::move(port));
            }
        }
        modules.push_back(std::move(module));
    }

    const auto* connectionArray = objectMember(root, "connections");
    if (!connectionArray || connectionArray->type != JsonValue::Type::Array) {
        return true;
    }

    for (std::size_t i = 0; i < connectionArray->arrayValue.size(); ++i) {
        const auto& connectionJson = connectionArray->arrayValue[i];
        Connection connection;
        connection.net = stringMember(connectionJson, "net", stringMember(connectionJson, "name", "net_" + std::to_string(i + 1)));
        if (const auto* from = objectMember(connectionJson, "from")) {
            connection.from = endpointFromJson(*from);
        } else if (const auto* source = objectMember(connectionJson, "source")) {
            connection.from = endpointFromJson(*source);
        }

        const auto* targets = objectMember(connectionJson, "to");
        if (!targets) {
            targets = objectMember(connectionJson, "targets");
        }
        if (!targets) {
            targets = objectMember(connectionJson, "sinks");
        }
        if (targets && targets->type == JsonValue::Type::Array) {
            for (const auto& target : targets->arrayValue) {
                auto endpoint = endpointFromJson(target);
                if (!endpoint.module.empty() && !endpoint.port.empty()) {
                    connection.to.push_back(std::move(endpoint));
                }
            }
        } else if (targets && targets->type == JsonValue::Type::Object) {
            auto endpoint = endpointFromJson(*targets);
            if (!endpoint.module.empty() && !endpoint.port.empty()) {
                connection.to.push_back(std::move(endpoint));
            }
        }
        if (!connection.from.module.empty() && !connection.from.port.empty()) {
            connections.push_back(std::move(connection));
        }
    }
    return true;
}

int portTextLength(const Port& port) {
    return static_cast<int>(port.name.size() + (port.width.empty() ? 0 : port.width.size() + 2));
}

void assignColumns(std::vector<Module>& modules, const std::vector<Connection>& connections) {
    std::map<std::string, std::size_t> byId;
    for (std::size_t i = 0; i < modules.size(); ++i) {
        byId[modules[i].id] = i;
    }

    for (std::size_t pass = 0; pass < modules.size(); ++pass) {
        bool changed = false;
        for (const auto& connection : connections) {
            const auto sourceIt = byId.find(connection.from.module);
            if (sourceIt == byId.end()) {
                continue;
            }
            for (const auto& target : connection.to) {
                const auto targetIt = byId.find(target.module);
                if (targetIt == byId.end()) {
                    continue;
                }
                if (sourceIt->second >= targetIt->second) {
                    continue;
                }
                const int nextColumn = std::max(modules[targetIt->second].column, modules[sourceIt->second].column + 1);
                if (nextColumn != modules[targetIt->second].column) {
                    modules[targetIt->second].column = nextColumn;
                    changed = true;
                }
            }
        }
        if (!changed) {
            break;
        }
    }
}

void assignPortPositions(Module& module) {
    constexpr int headerHeight = 34;
    constexpr int rowHeight = 24;
    for (std::size_t i = 0; i < module.inputs.size(); ++i) {
        auto& port = module.ports[module.inputs[i]];
        port.x = module.x;
        port.y = module.y + headerHeight + 16 + static_cast<int>(i) * rowHeight;
    }
    for (std::size_t i = 0; i < module.outputs.size(); ++i) {
        auto& port = module.ports[module.outputs[i]];
        port.x = module.x + module.width;
        port.y = module.y + headerHeight + 16 + static_cast<int>(i) * rowHeight;
    }
}

void layoutModules(std::vector<Module>& modules, const std::vector<Connection>& connections, int& width, int& height) {
    assignColumns(modules, connections);

    for (auto& module : modules) {
        int longestPort = 0;
        for (const auto& port : module.ports) {
            longestPort = std::max(longestPort, portTextLength(port));
        }
        const int rows = std::max<int>({static_cast<int>(module.inputs.size()), static_cast<int>(module.outputs.size()), 2});
        const int titleWidth = static_cast<int>(module.title.size()) * 8 + 44;
        const int portWidth = std::max(160, longestPort * 7 + 86);
        module.width = std::max({190, titleWidth, portWidth});
        module.height = 54 + rows * 24;
    }

    int maxColumn = 0;
    for (const auto& module : modules) {
        maxColumn = std::max(maxColumn, module.column);
    }

    constexpr int margin = 42;
    constexpr int columnGap = 280;
    constexpr int rowGap = 84;
    width = 900;
    height = 560;

    for (int column = 0; column <= maxColumn; ++column) {
        int y = margin;
        int maxColumnWidth = 190;
        for (const auto& module : modules) {
            if (module.column == column) {
                maxColumnWidth = std::max(maxColumnWidth, module.width);
            }
        }
        for (auto& module : modules) {
            if (module.column != column) {
                continue;
            }
            module.x = margin + column * columnGap;
            module.y = y;
            module.width = std::max(module.width, maxColumnWidth);
            assignPortPositions(module);
            width = std::max(width, module.x + module.width + margin);
            height = std::max(height, module.y + module.height + margin);
            y += module.height + rowGap;
        }
    }
}

const Module* findModule(const std::vector<Module>& modules, const std::string& id) {
    for (const auto& module : modules) {
        if (module.id == id) {
            return &module;
        }
    }
    return nullptr;
}

const Port* findPort(const Module& module, const std::string& name) {
    for (const auto& port : module.ports) {
        if (port.name == name) {
            return &port;
        }
    }
    return nullptr;
}

std::vector<std::pair<int, int>> route(const Port& source, const Port& target, int index) {
    if (target.x < source.x) {
        const int laneY = std::max(source.y, target.y) + 86 + index * 10;
        const int sourceLaneX = source.x + 22 + index * 4;
        const int targetLaneX = target.x - 22 - index * 4;
        return {
            {source.x, source.y},
            {sourceLaneX, source.y},
            {sourceLaneX, laneY},
            {targetLaneX, laneY},
            {targetLaneX, target.y},
            {target.x, target.y}
        };
    }

    const int laneOffset = ((index % 7) - 3) * 10;
    int midX = (source.x + target.x) / 2 + laneOffset;
    if (target.x >= source.x + 80) {
        midX = std::clamp(midX, source.x + 44, target.x - 44);
    } else {
        midX = std::max(source.x, target.x) + 86 + index * 8;
    }
    return {
        {source.x, source.y},
        {source.x + 22, source.y},
        {midX, source.y},
        {midX, target.y},
        {target.x - 22, target.y},
        {target.x, target.y}
    };
}

std::string endpointJson(const Endpoint& endpoint, const Port* port = nullptr) {
    std::ostringstream json;
    json << "{\"module\":\"" << jsonEscape(endpoint.module) << "\",\"port\":\"" << jsonEscape(endpoint.port) << "\"";
    if (port) {
        json << ",\"x\":" << port->x << ",\"y\":" << port->y;
    }
    json << "}";
    return json.str();
}

std::string layoutToJson(const std::string& name, const std::filesystem::path& source, const std::vector<Module>& modules,
                         const std::vector<Connection>& connections, int width, int height,
                         const std::string& compileJson = {}, bool leaf = false) {
    std::ostringstream json;
    json << "{\"name\":\"" << jsonEscape(name) << "\",\"source\":\"" << jsonEscape(source.string())
         << "\",\"width\":" << width << ",\"height\":" << height
         << ",\"leaf\":" << (leaf ? "true" : "false");
    if (!compileJson.empty()) {
        json << ",\"compile\":" << compileJson;
    }
    json << ",\"modules\":[";
    for (std::size_t i = 0; i < modules.size(); ++i) {
        const auto& module = modules[i];
        if (i != 0) {
            json << ',';
        }
        json << "{\"id\":\"" << jsonEscape(module.id) << "\",\"title\":\"" << jsonEscape(module.title)
             << "\",\"moduleName\":\"" << jsonEscape(module.moduleName)
             << "\",\"x\":" << module.x << ",\"y\":" << module.y
             << ",\"width\":" << module.width << ",\"height\":" << module.height << ",\"ports\":[";
        for (std::size_t p = 0; p < module.ports.size(); ++p) {
            const auto& port = module.ports[p];
            if (p != 0) {
                json << ',';
            }
            json << "{\"name\":\"" << jsonEscape(port.name) << "\",\"direction\":\"" << port.direction
                 << "\",\"width\":\"" << jsonEscape(port.width) << "\",\"x\":" << port.x << ",\"y\":" << port.y << "}";
        }
        json << "]}";
    }
    json << "],\"traces\":[";

    bool firstTrace = true;
    int traceIndex = 0;
    for (const auto& connection : connections) {
        const auto* sourceModule = findModule(modules, connection.from.module);
        const auto* sourcePort = sourceModule ? findPort(*sourceModule, connection.from.port) : nullptr;
        if (!sourcePort) {
            continue;
        }
        for (const auto& target : connection.to) {
            const auto* targetModule = findModule(modules, target.module);
            const auto* targetPort = targetModule ? findPort(*targetModule, target.port) : nullptr;
            if (!targetPort) {
                continue;
            }
            if (!firstTrace) {
                json << ',';
            }
            firstTrace = false;
            const auto points = route(*sourcePort, *targetPort, traceIndex++);
            const auto labelPoint = points[std::min<std::size_t>(2, points.size() - 1)];
            json << "{\"net\":\"" << jsonEscape(connection.net) << "\",\"from\":"
                 << endpointJson(connection.from, sourcePort) << ",\"to\":"
                 << endpointJson(target, targetPort) << ",\"label\":{\"x\":" << labelPoint.first + 8
                 << ",\"y\":" << labelPoint.second - 6 << "},\"points\":[";
            for (std::size_t p = 0; p < points.size(); ++p) {
                if (p != 0) {
                    json << ',';
                }
                json << "{\"x\":" << points[p].first << ",\"y\":" << points[p].second << "}";
            }
            json << "]}";
        }
    }
    json << "]}";
    return json.str();
}

} // namespace

namespace {

RPCSchematics::Result loadAndLayoutImpl(const std::filesystem::path& projectRoot,
                                        const std::filesystem::path& designFile,
                                        const std::string& compileJson,
                                        const std::string& moduleName,
                                        bool showTopBoundaryAsModule = false) {
    const auto source = designFile.is_absolute() ? designFile : projectRoot / designFile;
    std::ifstream file(source, std::ios::binary);
    if (!file) {
        return {false, {}, "design_not_found"};
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    JsonParser parser(stream.str());
    const auto root = parser.parse();
    if (!root) {
        return {false, {}, "invalid_design_json"};
    }

    std::string name;
    std::vector<Module> modules;
    std::vector<Connection> connections;
    if (!parseDesign(*root, moduleName, showTopBoundaryAsModule, name, modules, connections)) {
        return {false, {}, "invalid_design_shape"};
    }

    int width = 0;
    int height = 0;
    layoutModules(modules, connections, width, height);
    const bool leaf = !moduleName.empty() &&
        std::none_of(modules.begin(), modules.end(), [](const Module& module) {
            return !module.moduleName.empty();
        });
    return {true, layoutToJson(name, source, modules, connections, width, height, compileJson, leaf), {}};
}

} // namespace

RPCSchematics::Result RPCSchematics::loadAndLayout(const std::filesystem::path& projectRoot) {
    return loadAndLayoutImpl(projectRoot, "design.json", {}, {});
}

RPCSchematics::Result RPCSchematics::loadAndLayout(const std::filesystem::path& projectRoot,
                                                   const std::filesystem::path& designFile) {
    return loadAndLayoutImpl(projectRoot, designFile, {}, {});
}

RPCSchematics::Result RPCSchematics::loadAndLayout(const std::filesystem::path& projectRoot,
                                                   const std::filesystem::path& designFile,
                                                   const std::string& moduleName) {
    return loadAndLayoutImpl(projectRoot, designFile, {}, moduleName);
}

RPCSchematics::Result RPCSchematics::refreshAndLoad(const std::filesystem::path& projectRoot,
                                                    const std::filesystem::path& binDir,
                                                    const CompilationFlow& flow,
                                                    const std::string& projectName,
                                                    const std::string& topModuleName,
                                                    const std::string& topModuleFile,
                                                    const std::string& mainTestFile,
                                                    const std::string& additionalSources,
                                                    const std::string& stage,
                                                    const std::string& moduleName) {
    std::string normalizedStage = stage;
    if (normalizedStage.empty()) {
        normalizedStage = "Compilation";
    }
    std::string action = "Compile";
    std::filesystem::path outputFile = "design.json";
    std::string jsonOutputPath = "design.json";

    if (normalizedStage == "Test") {
        action = "Run";
        outputFile = ".trident/schematics_test.json";
        jsonOutputPath = outputFile.string();
    } else if (normalizedStage == "Synthesis") {
        action = flow.hasAction("Synthesis") ? "Synthesis" : "Synthesize";
        outputFile = "output.json";
        jsonOutputPath.clear();
    } else {
        action = flow.hasAction("Compilation") ? "Compilation" : "Compile";
        outputFile = "design.json";
        jsonOutputPath = outputFile.string();
    }

    const auto flowJson = flow.execute(
        action,
        projectRoot,
        binDir,
        projectName,
        topModuleName,
        topModuleFile,
        mainTestFile,
        additionalSources,
        jsonOutputPath);
    if (jsonIntValue(flowJson, "exitCode", -1) != 0) {
        return {false, flowJson, "flow_failed"};
    }
    return loadAndLayoutImpl(
        projectRoot,
        outputFile,
        flowJson,
        moduleName,
        normalizedStage == "Test");
}
