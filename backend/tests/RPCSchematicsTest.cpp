#include "RPCSchematicsTest.h"

#include "CompilationFlow.h"
#include "RPCSchematics.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

bool writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << content;
    return true;
}

std::filesystem::path uniqueTestDir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("trident_schematics_rpc_test_" + std::to_string(stamp));
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::filesystem::path findMemoryProjectExample() {
    std::error_code error;
    const auto cwd = std::filesystem::current_path(error);
    const std::filesystem::path candidates[] = {
        cwd / "package" / "MemoryPrj",
        cwd / ".." / "package" / "MemoryPrj",
        cwd / "../.." / "package" / "MemoryPrj"
    };
    for (const auto& candidate : candidates) {
        const auto canonical = std::filesystem::weakly_canonical(candidate, error);
        if (!error && std::filesystem::exists(canonical / "Memory.cpp", error) &&
                std::filesystem::exists(canonical / "design.json", error)) {
            return canonical;
        }
        error.clear();
    }
    return {};
}

} // namespace

bool RPCSchematicsTest::validate() {
    const auto projectRoot = uniqueTestDir();
    std::error_code error;
    std::filesystem::create_directories(projectRoot, error);
    if (error) {
        std::cerr << "RPCSchematicsTest: failed to create temp project\n";
        return false;
    }

    const auto cleanup = [&]() {
        std::error_code cleanupError;
        std::filesystem::remove_all(projectRoot, cleanupError);
    };

    const auto staleDesign = R"JSON({
  "name": "StaleTop",
  "modules": [
    {"id": "stale_src", "title": "StaleSrc", "ports": [{"name": "out", "direction": "output", "width": 1}]},
    {"id": "stale_dst", "title": "StaleDst", "ports": [{"name": "in", "direction": "input", "width": 1}]}
  ],
  "connections": [
    {"net": "stale_net", "from": {"module": "stale_src", "port": "out"}, "to": [{"module": "stale_dst", "port": "in"}]}
  ]
})JSON";
    if (!writeTextFile(projectRoot / "design.json", staleDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write stale design\n";
        return false;
    }

    const auto flowJson = R"FLOW({
  "actions": {
    "Compile": {
      "command": [
        "test \"$(TopModuleName)\" = \"FreshTop\"",
        "test \"$(TopModuleFile)\" = \"FreshTop.cpp\"",
        "cat > design.json <<'JSON'",
        "{\"creator\":\"cpphdl\",\"modules\":{\"FreshTop\":{\"attributes\":{\"top\":\"1\"},\"ports\":{\"clk\":{\"direction\":\"input\",\"bits\":[2]},\"a\":{\"direction\":\"input\",\"bits\":[3]},\"y\":{\"direction\":\"output\",\"bits\":[4]}},\"cells\":{\"u_new\":{\"hide_name\":0,\"type\":\"FreshChild\",\"port_directions\":{\"clk\":\"input\",\"a\":\"input\",\"y\":\"output\"},\"connections\":{\"clk\":[2],\"a\":[3],\"y\":[4]}}},\"netnames\":{\"clk\":{\"hide_name\":0,\"bits\":[2],\"attributes\":{}},\"a\":{\"hide_name\":0,\"bits\":[3],\"attributes\":{}},\"y\":{\"hide_name\":0,\"bits\":[4],\"attributes\":{}}}}}}",
        "JSON",
        "echo \"cpphdl FreshTop.cpp --json-output design.json\""
      ]
    }
  }
})FLOW";
    const auto flowPath = projectRoot / "flow.json";
    if (!writeTextFile(flowPath, flowJson)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write flow\n";
        return false;
    }

    CompilationFlow flow;
    if (!flow.load(flowPath)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to load flow\n";
        return false;
    }

    const auto result = RPCSchematics::refreshAndLoad(
        projectRoot,
        projectRoot,
        flow,
        "SchematicsTest",
        "FreshTop",
        "FreshTop.cpp",
        "main.cpp",
        "");

    const auto regeneratedDesign = readTextFile(projectRoot / "design.json");
    const bool ok = result.ok &&
        contains(result.json, "\"name\":\"FreshTop\"") &&
        contains(result.json, "\"id\":\"u_new\"") &&
        contains(result.json, "\"id\":\"__top_in__a\",\"title\":\"input\"") &&
        contains(result.json, "\"id\":\"__top_out__y\",\"title\":\"output\"") &&
        contains(result.json, "--json-output design.json") &&
        contains(regeneratedDesign, "\"creator\":\"cpphdl\"") &&
        !contains(result.json, "StaleTop") &&
        !contains(regeneratedDesign, "StaleTop");

    if (!ok) {
        std::cerr << "RPCSchematicsTest failed\n";
        std::cerr << "result.ok=" << result.ok << " error=" << result.error << "\n";
        std::cerr << result.json << "\n";
        cleanup();
        return false;
    }

    const auto noCellDesign = R"JSON({
  "creator": "cpphdl",
  "modules": {
    "Memory": {
      "attributes": {},
      "ports": {
        "addr_in": {"direction": "input", "bits": [4, 5, 6, 7]},
        "read_data_out": {"direction": "output", "bits": [8, 9, 10, 11]}
      },
      "cells": {},
      "netnames": {}
    }
  }
})JSON";
    if (!writeTextFile(projectRoot / "design.json", noCellDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write no-cell design\n";
        return false;
    }
    const auto noCellResult = RPCSchematics::loadAndLayout(projectRoot);
    const bool noCellOk = noCellResult.ok &&
        contains(noCellResult.json, "\"name\":\"Memory\"") &&
        contains(noCellResult.json, "\"id\":\"Memory\",\"title\":\"Memory\"") &&
        contains(noCellResult.json, "\"name\":\"addr_in\"") &&
        contains(noCellResult.json, "\"name\":\"read_data_out\"") &&
        !contains(noCellResult.json, "__top_in__addr_in") &&
        !contains(noCellResult.json, "__top_out__read_data_out");
    if (!noCellOk) {
        std::cerr << "RPCSchematicsTest no-cell design failed\n";
        std::cerr << "result.ok=" << noCellResult.ok << " error=" << noCellResult.error << "\n";
        std::cerr << noCellResult.json << "\n";
        cleanup();
        return false;
    }

    const auto cpphdlDirectPortDesign = R"JSON({
  "creator": "cpphdl",
  "modules": {
    "MemoryTestHarness": {
      "attributes": {"top": "1"},
      "ports": {},
      "cells": {
        "memory": {
          "hide_name": 0,
          "type": "Memory",
          "port_directions": {
            "addr_in": "input",
            "write_in": "input",
            "read_data_out": "output"
          },
          "connections": {
            "addr_in": [4, 5, 6, 7],
            "write_in": [8],
            "read_data_out": [46, 47, 48, 49]
          }
        },
        "test": {
          "hide_name": 0,
          "type": "MemoryTest",
          "port_directions": {
            "addr_out": "output",
            "write_out": "output",
            "read_data_in": "input"
          },
          "connections": {
            "addr_out": [79, 80, 81, 82],
            "write_out": [83],
            "read_data_in": [121, 122, 123, 124]
          }
        }
      },
      "netnames": {}
    }
  }
})JSON";
    if (!writeTextFile(projectRoot / "design.json", cpphdlDirectPortDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write cpphdl direct-port design\n";
        return false;
    }
    const auto cpphdlDirectPortResult = RPCSchematics::loadAndLayout(projectRoot);
    const bool cpphdlDirectPortOk = cpphdlDirectPortResult.ok &&
        contains(cpphdlDirectPortResult.json, "\"name\":\"MemoryTestHarness\"") &&
        contains(cpphdlDirectPortResult.json, "\"id\":\"memory\",\"title\":\"Memory\"") &&
        contains(cpphdlDirectPortResult.json, "\"id\":\"test\",\"title\":\"MemoryTest\"") &&
        contains(cpphdlDirectPortResult.json, "\"net\":\"addr\"") &&
        contains(cpphdlDirectPortResult.json, "\"net\":\"write\"") &&
        contains(cpphdlDirectPortResult.json, "\"net\":\"read_data\"") &&
        contains(cpphdlDirectPortResult.json, "\"module\":\"test\",\"port\":\"addr_out\"") &&
        contains(cpphdlDirectPortResult.json, "\"module\":\"memory\",\"port\":\"addr_in\"") &&
        contains(cpphdlDirectPortResult.json, "\"module\":\"memory\",\"port\":\"read_data_out\"") &&
        contains(cpphdlDirectPortResult.json, "\"module\":\"test\",\"port\":\"read_data_in\"");
    if (!cpphdlDirectPortOk) {
        std::cerr << "RPCSchematicsTest cpphdl direct-port design failed\n";
        std::cerr << "result.ok=" << cpphdlDirectPortResult.ok << " error=" << cpphdlDirectPortResult.error << "\n";
        std::cerr << cpphdlDirectPortResult.json << "\n";
        cleanup();
        return false;
    }

    const auto primitiveOnlyDesign = R"JSON({
  "creator": "Yosys",
  "modules": {
    "Memory": {
      "attributes": {"top": "1"},
      "ports": {
        "addr_in": {"direction": "input", "bits": [4, 5, 6, 7]},
        "read_data_out": {"direction": "output", "bits": [8, 9, 10, 11]}
      },
      "cells": {
        "$1": {
          "hide_name": 1,
          "type": "$not",
          "port_directions": {"A": "input", "Y": "output"},
          "connections": {"A": [4], "Y": [8]}
        }
      },
      "netnames": {}
    }
  }
})JSON";
    if (!writeTextFile(projectRoot / "output.json", primitiveOnlyDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write primitive-only design\n";
        return false;
    }
    const auto primitiveOnlyResult = RPCSchematics::loadAndLayout(projectRoot, "output.json");
    const bool primitiveOnlyOk = primitiveOnlyResult.ok &&
        contains(primitiveOnlyResult.json, "\"id\":\"Memory\",\"title\":\"Memory\"") &&
        contains(primitiveOnlyResult.json, "\"name\":\"addr_in\"") &&
        !contains(primitiveOnlyResult.json, "\"id\":\"$1\"");
    if (!primitiveOnlyOk) {
        std::cerr << "RPCSchematicsTest primitive-only design failed\n";
        std::cerr << "result.ok=" << primitiveOnlyResult.ok << " error=" << primitiveOnlyResult.error << "\n";
        std::cerr << primitiveOnlyResult.json << "\n";
        cleanup();
        return false;
    }

    const auto hierarchicalDesign = R"JSON({
  "creator": "Yosys",
  "modules": {
    "Top": {
      "attributes": {"top": "1"},
      "ports": {
        "a": {"direction": "input", "bits": [2]},
        "y": {"direction": "output", "bits": [3]}
      },
      "cells": {
        "u_child": {
          "hide_name": 0,
          "type": "Child",
          "port_directions": {"a": "input", "y": "output"},
          "connections": {"a": [2], "y": [3]}
        }
      },
      "netnames": {
        "a": {"hide_name": 0, "bits": [2], "attributes": {}},
        "y": {"hide_name": 0, "bits": [3], "attributes": {}}
      }
    },
    "Child": {
      "attributes": {},
      "data_members": {
        "state_reg": {"kind": "register", "width": 8},
        "ready_comb": {"kind": "combinational", "width": 1}
      },
      "ports": {
        "a": {"direction": "input", "bits": [2]},
        "y": {"direction": "output", "bits": [3]}
      },
      "cells": {},
      "netnames": {
        "a": {"hide_name": 0, "bits": [2], "attributes": {}},
        "y": {"hide_name": 0, "bits": [3], "attributes": {}}
      }
    }
  }
})JSON";
    if (!writeTextFile(projectRoot / "design.json", hierarchicalDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write hierarchical design\n";
        return false;
    }
    const auto topHierarchyResult = RPCSchematics::loadAndLayout(projectRoot);
    const bool topHierarchyOk = topHierarchyResult.ok &&
        contains(topHierarchyResult.json, "\"name\":\"Top\"") &&
        contains(topHierarchyResult.json, "\"leaf\":false") &&
        contains(topHierarchyResult.json, "\"id\":\"u_child\",\"title\":\"Child\",\"moduleName\":\"Child\"");
    const auto childHierarchyResult = RPCSchematics::loadAndLayout(projectRoot, "design.json", "Child");
    const bool childHierarchyOk = childHierarchyResult.ok &&
        contains(childHierarchyResult.json, "\"name\":\"Child\"") &&
        contains(childHierarchyResult.json, "\"leaf\":true") &&
        contains(childHierarchyResult.json, "\"id\":\"Child\",\"title\":\"Child\"") &&
        contains(childHierarchyResult.json, "\"id\":\"__member__state_reg\",\"title\":\"state_reg : register [8]\"") &&
        contains(childHierarchyResult.json, "\"id\":\"__member__ready_comb\",\"title\":\"ready_comb : combinational [1]\"") &&
        contains(childHierarchyResult.json, "\"name\":\"a\"") &&
        contains(childHierarchyResult.json, "\"name\":\"y\"") &&
        !contains(childHierarchyResult.json, "u_child");
    if (!topHierarchyOk || !childHierarchyOk) {
        std::cerr << "RPCSchematicsTest hierarchical drill-down failed\n";
        std::cerr << "top.ok=" << topHierarchyResult.ok << " top.error=" << topHierarchyResult.error << "\n";
        std::cerr << topHierarchyResult.json << "\n";
        std::cerr << "child.ok=" << childHierarchyResult.ok << " child.error=" << childHierarchyResult.error << "\n";
        std::cerr << childHierarchyResult.json << "\n";
        cleanup();
        return false;
    }

    const auto cpphdlLeafDesign = R"JSON({
  "creator": "cpphdl",
  "modules": {
    "Leaf": {
      "attributes": {"top": "1"},
      "ports": {
        "bus_in": {"direction": "input", "kind": "interface", "bits": [2, 3]},
        "result_out": {"direction": "output", "kind": "struct", "bits": [4, 5]}
      },
      "cells": {},
      "data_members": {
        "state_reg": {"kind": "register", "width": 8}
      },
      "netnames": {}
    }
  }
})JSON";
    if (!writeTextFile(projectRoot / "design.json", cpphdlLeafDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write cpphdl leaf design\n";
        return false;
    }
    const auto cpphdlLeafTop = RPCSchematics::loadAndLayout(projectRoot);
    const auto cpphdlLeafDrill = RPCSchematics::loadAndLayout(projectRoot, "design.json", "Leaf");
    const bool cpphdlLeafOk = cpphdlLeafTop.ok && cpphdlLeafDrill.ok &&
        contains(cpphdlLeafTop.json, "\"id\":\"Leaf\",\"title\":\"Leaf\",\"moduleName\":\"Leaf\"") &&
        contains(cpphdlLeafDrill.json, "\"leaf\":true") &&
        contains(cpphdlLeafDrill.json, "\"id\":\"__member__state_reg\",\"title\":\"state_reg : register [8]\"");
    if (!cpphdlLeafOk) {
        std::cerr << "RPCSchematicsTest cpphdl leaf drill-down failed\n";
        std::cerr << cpphdlLeafTop.json << "\n" << cpphdlLeafDrill.json << "\n";
        cleanup();
        return false;
    }

    const auto bufferTestDesign = R"JSON({
  "creator": "cpphdl",
  "modules": {
    "Buffer": {
      "attributes": {},
      "ports": {
        "valid_in": {"direction": "input", "bits": [4]},
        "data_in": {"direction": "input", "bits": [5]},
        "ready_out": {"direction": "output", "bits": [6]},
        "valid_out": {"direction": "output", "bits": [7]},
        "data_out": {"direction": "output", "bits": [8]},
        "ready_in": {"direction": "input", "bits": [9]}
      },
      "cells": {},
      "netnames": {}
    },
    "TestBuffer": {
      "attributes": {},
      "ports": {
        "debugen_in": {"direction": "input", "bits": [4]}
      },
      "cells": {
        "dut": {
          "hide_name": 0,
          "type": "Buffer",
          "port_directions": {
            "valid_in": "input",
            "data_in": "input",
            "ready_out": "output",
            "valid_out": "output",
            "data_out": "output",
            "ready_in": "input"
          },
          "connections": {
            "valid_in": [5],
            "data_in": [6],
            "ready_out": [7],
            "valid_out": [8],
            "data_out": [9],
            "ready_in": [10]
          }
        }
      },
      "netnames": {
        "debugen_in": {"hide_name": 0, "bits": [4], "attributes": {}},
        "data_in_reg": {"hide_name": 0, "bits": [6], "attributes": {}},
        "data_out_value": {"hide_name": 0, "bits": [9], "attributes": {}},
        "dut__valid_in": {"hide_name": 0, "bits": [5], "attributes": {}},
        "dut__data_in": {"hide_name": 0, "bits": [6], "attributes": {}},
        "dut__ready_out": {"hide_name": 0, "bits": [7], "attributes": {}},
        "dut__valid_out": {"hide_name": 0, "bits": [8], "attributes": {}},
        "dut__data_out": {"hide_name": 0, "bits": [9], "attributes": {}},
        "dut__ready_in": {"hide_name": 0, "bits": [10], "attributes": {}},
        "ready_in_reg": {"hide_name": 0, "bits": [10], "attributes": {}},
        "ready_out_value": {"hide_name": 0, "bits": [7], "attributes": {}},
        "valid_in_reg": {"hide_name": 0, "bits": [5], "attributes": {}},
        "valid_out_value": {"hide_name": 0, "bits": [8], "attributes": {}}
      }
    }
  }
})JSON";
    if (!writeTextFile(projectRoot / "buffer_test_design.json", bufferTestDesign)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write Buffer test design\n";
        return false;
    }
    const auto bufferFlowJson = R"FLOW({
  "actions": {
    "Run": {
      "command": [
        "test -n \"$(JsonOutputPath)\"",
        "mkdir -p .trident",
        "cp buffer_test_design.json \"$(JsonOutputPath)\""
      ]
    }
  }
})FLOW";
    const auto bufferFlowPath = projectRoot / "buffer_flow.json";
    if (!writeTextFile(bufferFlowPath, bufferFlowJson)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write Buffer test flow\n";
        return false;
    }
    CompilationFlow bufferFlow;
    if (!bufferFlow.load(bufferFlowPath)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to load Buffer test flow\n";
        return false;
    }
    const auto bufferResult = RPCSchematics::refreshAndLoad(
        projectRoot,
        projectRoot,
        bufferFlow,
        "Buffer",
        "Buffer",
        "Buffer.cpp",
        "Buffer.cpp",
        "",
        "Test");
    const bool bufferOk = bufferResult.ok &&
        contains(bufferResult.json, "\"name\":\"TestBuffer\"") &&
        contains(bufferResult.json, "\"id\":\"__top__TestBuffer\",\"title\":\"TestBuffer\"") &&
        contains(bufferResult.json, "\"id\":\"dut\",\"title\":\"Buffer\"") &&
        !contains(bufferResult.json, "\"title\":\"input\"") &&
        contains(bufferResult.json, "\"name\":\"debugen_in\",\"direction\":\"input\"") &&
        contains(bufferResult.json, "\"net\":\"valid_in_reg\"") &&
        contains(bufferResult.json, "\"net\":\"data_in_reg\"") &&
        contains(bufferResult.json, "\"net\":\"ready_in_reg\"") &&
        contains(bufferResult.json, "\"net\":\"ready_out_value\"") &&
        contains(bufferResult.json, "\"net\":\"valid_out_value\"") &&
        contains(bufferResult.json, "\"net\":\"data_out_value\"") &&
        contains(bufferResult.json, "\"module\":\"__top__TestBuffer\"") &&
        contains(bufferResult.json, "\"module\":\"dut\"") &&
        contains(bufferResult.json,
            "\"from\":{\"module\":\"__top__TestBuffer\",\"port\":\"data_in_reg\",\"x\":254,\"y\":92},"
            "\"to\":{\"module\":\"dut\",\"port\":\"data_in\",\"x\":322,\"y\":92}") &&
        contains(bufferResult.json,
            "\"from\":{\"module\":\"dut\",\"port\":\"data_out\",\"x\":512,\"y\":92},"
            "\"to\":{\"module\":\"__top__TestBuffer\",\"port\":\"data_out_value\",\"x\":42,\"y\":92}");
    if (!bufferOk) {
        std::cerr << "RPCSchematicsTest current Buffer test shape failed\n";
        std::cerr << "result.ok=" << bufferResult.ok << " error=" << bufferResult.error << "\n";
        std::cerr << bufferResult.json << "\n";
        cleanup();
        return false;
    }

    const auto memoryExample = findMemoryProjectExample();
    if (memoryExample.empty()) {
        cleanup();
        std::cerr << "RPCSchematicsTest: package/MemoryPrj example not found\n";
        return false;
    }

    const auto memoryProject = projectRoot / "MemoryPrj";
    std::filesystem::copy(memoryExample, memoryProject,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to copy package/MemoryPrj\n";
        return false;
    }
    if (!writeTextFile(memoryProject / "reference_design.json", readTextFile(memoryProject / "design.json"))) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write Memory reference design\n";
        return false;
    }
    const auto memoryHarnessReference = R"JSON({
  "creator": "cpphdl",
  "modules": {
    "MemoryTestHarness": {
      "attributes": {"top": "1"},
      "ports": {},
      "cells": {
        "memory": {
          "hide_name": 0,
          "type": "Memory",
          "port_directions": {
            "addr_in": "input",
            "write_in": "input",
            "read_data_out": "output"
          },
          "connections": {
            "addr_in": [4, 5, 6, 7],
            "write_in": [8],
            "read_data_out": [46, 47, 48, 49]
          }
        },
        "test": {
          "hide_name": 0,
          "type": "MemoryTest",
          "port_directions": {
            "addr_out": "output",
            "write_out": "output",
            "read_data_in": "input"
          },
          "connections": {
            "addr_out": [79, 80, 81, 82],
            "write_out": [83],
            "read_data_in": [121, 122, 123, 124]
          }
        }
      },
      "netnames": {}
    }
  }
})JSON";
    if (!writeTextFile(memoryProject / "reference_test_design.json", memoryHarnessReference)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write Memory harness reference design\n";
        return false;
    }

    const auto memoryFlowJson = R"FLOW({
  "actions": {
    "Run": {
      "command": [
        "test -n \"$(JsonOutputPath)\"",
        "mkdir -p .trident",
        "cp reference_test_design.json \"$(JsonOutputPath)\"",
        "echo \"Run Memory with --json-output $(JsonOutputPath)\""
      ]
    },
    "Compilation": {
      "command": [
        "test -n \"$(JsonOutputPath)\"",
        "cp reference_design.json \"$(JsonOutputPath)\"",
        "echo \"Compilation Memory with --json-output $(JsonOutputPath)\""
      ]
    },
    "Synthesis": {
      "command": [
        "cp reference_design.json output.json",
        "echo \"Synthesis Memory to output.json\""
      ]
    }
  }
})FLOW";
    const auto memoryFlowPath = memoryProject / "flow.json";
    if (!writeTextFile(memoryFlowPath, memoryFlowJson)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to write Memory flow\n";
        return false;
    }

    CompilationFlow memoryFlow;
    if (!memoryFlow.load(memoryFlowPath)) {
        cleanup();
        std::cerr << "RPCSchematicsTest: failed to load Memory flow\n";
        return false;
    }

    const auto validateStage = [&](const std::string& stage, const std::string& expectedAction) {
        const auto stageResult = RPCSchematics::refreshAndLoad(
            memoryProject,
            memoryProject,
            memoryFlow,
            "MemoryPrj",
            "Memory",
            "Memory.cpp",
            "MemoryTest.cpp",
            "",
            stage);
        bool stageOk = stageResult.ok &&
            contains(stageResult.json, "\"action\":\"" + expectedAction + "\"");
        if (stage == "Test") {
            stageOk = stageOk &&
                contains(stageResult.json, "\"name\":\"MemoryTestHarness\"") &&
                contains(stageResult.json, "\"id\":\"memory\",\"title\":\"Memory\"") &&
                contains(stageResult.json, "\"id\":\"test\",\"title\":\"MemoryTest\"") &&
                contains(stageResult.json, "\"net\":\"addr") &&
                contains(stageResult.json, "\"net\":\"write") &&
                contains(stageResult.json, "\"net\":\"read_data");
        } else {
            stageOk = stageOk &&
                contains(stageResult.json, "\"name\":\"Memory\"") &&
                contains(stageResult.json, "\"id\":\"Memory\",\"title\":\"Memory\"") &&
                contains(stageResult.json, "\"name\":\"addr_in\"") &&
                contains(stageResult.json, "\"name\":\"read_data_out\"");
        }
        if (!stageOk) {
            std::cerr << "RPCSchematicsTest Memory stage failed: " << stage << "\n";
            std::cerr << "result.ok=" << stageResult.ok << " error=" << stageResult.error << "\n";
            std::cerr << stageResult.json << "\n";
        }
        return stageOk;
    };

    if (!validateStage("Test", "Run") ||
            !validateStage("Compilation", "Compilation") ||
            !validateStage("Synthesis", "Synthesis")) {
        cleanup();
        return false;
    }

    cleanup();
    return true;
}
