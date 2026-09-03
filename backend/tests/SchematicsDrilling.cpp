#include "SchematicsDrilling.h"

#include "CompilationFlow.h"
#include "RPCSchematics.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

bool writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << content;
    return true;
}

std::filesystem::path uniqueTestDir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("trident_schematics_drilling_test_" + std::to_string(stamp));
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

const char* hierarchicalDesignJson() {
    return R"JSON({
  "creator": "Yosys",
  "modules": {
    "Top": {
      "attributes": {"top": "1"},
      "ports": {
        "clk": {"direction": "input", "bits": [2]},
        "result": {"direction": "output", "bits": [3]}
      },
      "cells": {
        "u_mid": {
          "hide_name": 0,
          "type": "Mid",
          "port_directions": {"clk": "input", "result": "output"},
          "connections": {"clk": [2], "result": [3]}
        }
      },
      "netnames": {
        "clk": {"hide_name": 0, "bits": [2], "attributes": {}},
        "result": {"hide_name": 0, "bits": [3], "attributes": {}}
      }
    },
    "Mid": {
      "attributes": {},
      "ports": {
        "clk": {"direction": "input", "bits": [2]},
        "result": {"direction": "output", "bits": [3]}
      },
      "cells": {
        "u_leaf": {
          "hide_name": 0,
          "type": "Leaf",
          "port_directions": {"clk": "input", "result": "output"},
          "connections": {"clk": [2], "result": [3]}
        },
        "$internal_mux": {
          "hide_name": 1,
          "type": "$mux",
          "port_directions": {"A": "input", "B": "input", "Y": "output"},
          "connections": {"A": [2], "B": [3], "Y": [4]}
        }
      },
      "netnames": {
        "clk": {"hide_name": 0, "bits": [2], "attributes": {}},
        "result": {"hide_name": 0, "bits": [3], "attributes": {}}
      }
    },
    "Leaf": {
      "attributes": {},
      "ports": {
        "clk": {"direction": "input", "bits": [2]},
        "result": {"direction": "output", "bits": [3]}
      },
      "cells": {
        "$dff": {
          "hide_name": 1,
          "type": "$dff",
          "port_directions": {"CLK": "input", "D": "input", "Q": "output"},
          "connections": {"CLK": [2], "D": [3], "Q": [3]}
        }
      },
      "netnames": {
        "clk": {"hide_name": 0, "bits": [2], "attributes": {}},
        "result": {"hide_name": 0, "bits": [3], "attributes": {}}
      }
    }
  }
})JSON";
}

} // namespace

bool SchematicsDrilling::validate() {
    const auto projectRoot = uniqueTestDir();
    std::error_code error;
    std::filesystem::create_directories(projectRoot, error);
    if (error) {
        std::cerr << "SchematicsDrilling: failed to create temp project\n";
        return false;
    }

    const auto cleanup = [&]() {
        std::error_code cleanupError;
        std::filesystem::remove_all(projectRoot, cleanupError);
    };

    if (!writeTextFile(projectRoot / "reference_design.json", hierarchicalDesignJson())) {
        cleanup();
        std::cerr << "SchematicsDrilling: failed to write reference design\n";
        return false;
    }

    const auto flowJson = R"FLOW({
  "actions": {
    "Compilation": {
      "command": [
        "test -n \"$(JsonOutputPath)\"",
        "cp reference_design.json \"$(JsonOutputPath)\"",
        "echo \"Compilation drilling flow for $(JsonOutputPath)\""
      ]
    }
  }
})FLOW";
    const auto flowPath = projectRoot / "flow.json";
    if (!writeTextFile(flowPath, flowJson)) {
        cleanup();
        std::cerr << "SchematicsDrilling: failed to write flow\n";
        return false;
    }

    CompilationFlow flow;
    if (!flow.load(flowPath)) {
        cleanup();
        std::cerr << "SchematicsDrilling: failed to load flow\n";
        return false;
    }

    const auto load = [&](const std::string& moduleName) {
        return RPCSchematics::refreshAndLoad(
            projectRoot,
            projectRoot,
            flow,
            "DrillProject",
            "Top",
            "Top.cpp",
            "main.cpp",
            "",
            "Compilation",
            moduleName);
    };

    const auto top = load("");
    const bool topOk = top.ok &&
        contains(top.json, "\"name\":\"Top\"") &&
        contains(top.json, "\"id\":\"u_mid\",\"title\":\"Mid\",\"moduleName\":\"Mid\"") &&
        !contains(top.json, "u_leaf");
    if (!topOk) {
        std::cerr << "SchematicsDrilling: top load failed\n";
        std::cerr << "ok=" << top.ok << " error=" << top.error << "\n" << top.json << "\n";
        cleanup();
        return false;
    }

    const auto mid = load("Mid");
    const bool midOk = mid.ok &&
        contains(mid.json, "\"name\":\"Mid\"") &&
        contains(mid.json, "\"id\":\"u_leaf\",\"title\":\"Leaf\",\"moduleName\":\"Leaf\"") &&
        !contains(mid.json, "\"id\":\"u_mid\"") &&
        !contains(mid.json, "$internal_mux");
    if (!midOk) {
        std::cerr << "SchematicsDrilling: Mid drill failed\n";
        std::cerr << "ok=" << mid.ok << " error=" << mid.error << "\n" << mid.json << "\n";
        cleanup();
        return false;
    }

    const auto leaf = load("Leaf");
    const bool leafOk = leaf.ok &&
        contains(leaf.json, "\"name\":\"Leaf\"") &&
        contains(leaf.json, "\"id\":\"Leaf\",\"title\":\"Leaf\",\"moduleName\":\"\"") &&
        contains(leaf.json, "\"name\":\"clk\"") &&
        contains(leaf.json, "\"name\":\"result\"") &&
        !contains(leaf.json, "\"id\":\"$dff\"");
    if (!leafOk) {
        std::cerr << "SchematicsDrilling: Leaf drill failed\n";
        std::cerr << "ok=" << leaf.ok << " error=" << leaf.error << "\n" << leaf.json << "\n";
        cleanup();
        return false;
    }

    const auto missing = load("MissingModule");
    if (missing.ok || missing.error != "invalid_design_shape") {
        std::cerr << "SchematicsDrilling: missing module should fail clearly\n";
        std::cerr << "ok=" << missing.ok << " error=" << missing.error << "\n" << missing.json << "\n";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}
