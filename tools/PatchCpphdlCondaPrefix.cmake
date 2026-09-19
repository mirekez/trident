cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CPPHDL_SOURCE_DIR)
    message(FATAL_ERROR "CPPHDL_SOURCE_DIR is required")
endif()

set(cmake_file "${CPPHDL_SOURCE_DIR}/CMakeLists.txt")
file(READ "${cmake_file}" cmake_text)
if(NOT cmake_text MATCHES "# TRIDENT_CPPHDL_CONDA_PREFIX")
    # The pinned revision only searches its own source tree for .conda, even
    # though Trident supplies CPPHDL_LOCAL_CONDA_PREFIX from its tool environment.
    set(original_prefix [=[${CMAKE_SOURCE_DIR}/.conda]=])
    set(selected_prefix [=[${CPPHDL_LOCAL_CONDA_PREFIX}]=])
    string(FIND "${cmake_text}" "${original_prefix}" prefix_pos)
    if(prefix_pos EQUAL -1)
        message(FATAL_ERROR "Could not locate CppHDL's local Conda paths in ${cmake_file}")
    endif()
    string(REPLACE "${original_prefix}" "${selected_prefix}" cmake_text "${cmake_text}")

    set(anchor "set(CPPHDL_USE_LOCAL_CONDA ON CACHE BOOL")
    string(FIND "${cmake_text}" "${anchor}" anchor_pos)
    if(anchor_pos EQUAL -1)
        message(FATAL_ERROR "Could not locate CppHDL's Conda option in ${cmake_file}")
    endif()
    set(prefix_option [=[# TRIDENT_CPPHDL_CONDA_PREFIX: share the parent project's environment.
set(CPPHDL_LOCAL_CONDA_PREFIX "${CMAKE_SOURCE_DIR}/.conda" CACHE PATH
    "Conda prefix providing the CppHDL toolchain and LLVM/Clang")

]=])
    string(REPLACE "${anchor}" "${prefix_option}${anchor}" cmake_text "${cmake_text}")
endif()

# Also upgrade source trees patched by the earlier version of this script.
if(NOT cmake_text MATCHES "# TRIDENT_CPPHDL_CONDA_LIBRARY")
    set(anchor "set(CPPHDL_USE_LOCAL_CONDA ON CACHE BOOL")
    string(FIND "${cmake_text}" "${anchor}" anchor_pos)
    if(anchor_pos EQUAL -1)
        message(FATAL_ERROR "Could not locate CppHDL's Conda option in ${cmake_file}")
    endif()
    set(library_prefix [=[# TRIDENT_CPPHDL_CONDA_LIBRARY: Windows Conda packages live under Library.
file(TO_CMAKE_PATH "${CPPHDL_LOCAL_CONDA_PREFIX}" CPPHDL_LOCAL_CONDA_PREFIX)
if(NOT EXISTS "${CPPHDL_LOCAL_CONDA_PREFIX}/include/clang/Tooling/Tooling.h"
    AND EXISTS "${CPPHDL_LOCAL_CONDA_PREFIX}/Library/include/clang/Tooling/Tooling.h")
  string(APPEND CPPHDL_LOCAL_CONDA_PREFIX "/Library")
endif()

]=])
    string(REPLACE "${anchor}" "${library_prefix}${anchor}" cmake_text "${cmake_text}")
endif()

file(READ "${cmake_file}" original_text)
if(cmake_text STREQUAL original_text)
    message(STATUS "CppHDL Conda prefix already patched")
    return()
endif()
file(WRITE "${cmake_file}" "${cmake_text}")
message(STATUS "Patched CppHDL Conda prefix and Windows Library layout")
