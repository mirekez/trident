cmake_minimum_required(VERSION 3.10)

message(STATUS "Patching Yosys CMake for Windows")

if(NOT DEFINED YOSYS_SOURCE_DIR)
    message(FATAL_ERROR "YOSYS_SOURCE_DIR is required")
endif()

file(TO_CMAKE_PATH "${YOSYS_SOURCE_DIR}" yosys_source_dir)
message(STATUS "  source: ${yosys_source_dir}")

set(yosys_cmake "${yosys_source_dir}/CMakeLists.txt")
if(NOT EXISTS "${yosys_cmake}")
    message(FATAL_ERROR "Missing ${yosys_cmake}")
endif()

file(READ "${yosys_cmake}" yosys_cmake_content)

set(old_line "cmake_minimum_required(VERSION 3.27)")
set(new_line "cmake_minimum_required(VERSION 3.26)")

string(FIND "${yosys_cmake_content}" "${old_line}" old_line_pos)
string(FIND "${yosys_cmake_content}" "${new_line}" new_line_pos)

if(NOT old_line_pos EQUAL -1)
    string(REPLACE "${old_line}" "${new_line}" patched_yosys_cmake_content "${yosys_cmake_content}")
    file(WRITE "${yosys_cmake}" "${patched_yosys_cmake_content}")
    message(STATUS "  patched: 3.27 -> 3.26")
else()
    if(NOT new_line_pos EQUAL -1)
        message(STATUS "  already patched")
    else()
        message(FATAL_ERROR "Could not find ${old_line} in ${yosys_cmake}")
    endif()
endif()
