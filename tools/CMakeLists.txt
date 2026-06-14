set (_TRIDENT_CONDA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.conda")
if(NOT EXISTS "${_TRIDENT_CONDA_ROOT}" AND DEFINED ENV{CONDA_PREFIX})
file(TO_CMAKE_PATH "$ENV{CONDA_PREFIX}" _TRIDENT_CONDA_ROOT)
endif()
file(TO_CMAKE_PATH "${_TRIDENT_CONDA_ROOT}" _TRIDENT_CONDA_ROOT)

if(WIN32 AND EXISTS "${_TRIDENT_CONDA_ROOT}/Library/bin/x86_64-w64-mingw32-g++.exe")
set (_TRIDENT_CONDA_BIN "${_TRIDENT_CONDA_ROOT}/Library/bin")
else()
set (_TRIDENT_CONDA_BIN "${_TRIDENT_CONDA_ROOT}/bin")
endif()
file(TO_CMAKE_PATH "${_TRIDENT_CONDA_BIN}" _TRIDENT_CONDA_BIN)

if(WIN32)
set (CMAKE_LINKER "${_TRIDENT_CONDA_BIN}/ld64.lld.exe")
set (CMAKE_C_COMPILER "${_TRIDENT_CONDA_BIN}/x86_64-w64-mingw32-gcc.exe")
set (CMAKE_CXX_COMPILER "${_TRIDENT_CONDA_BIN}/x86_64-w64-mingw32-g++.exe")
set (CMAKE_GENERATOR "Unix Makefiles")
set (_TRIDENT_MAKE_PROGRAM "${_TRIDENT_CONDA_BIN}/make.exe")
foreach(_TRIDENT_MAKE_CANDIDATE
    "${_TRIDENT_CONDA_ROOT}/Library/bin/make.exe"
    "${_TRIDENT_CONDA_ROOT}/Library/usr/bin/make.exe"
    "${_TRIDENT_CONDA_ROOT}/bin/make.exe"
)
    if(EXISTS "${_TRIDENT_MAKE_CANDIDATE}")
        set (_TRIDENT_MAKE_PROGRAM "${_TRIDENT_MAKE_CANDIDATE}")
        break()
    endif()
endforeach()
set (CMAKE_MAKE_PROGRAM "${_TRIDENT_MAKE_PROGRAM}" CACHE FILEPATH "Make program" FORCE)
set (CMAKE_C_COMPILER_FORCED "TRUE")
set (CMAKE_CXX_COMPILER_FORCED "TRUE")
else()
set (CMAKE_LINKER "${_TRIDENT_CONDA_BIN}/x86_64-conda-linux-gnu-ld")
set (CMAKE_C_COMPILER "${_TRIDENT_CONDA_BIN}/x86_64-conda-linux-gnu-gcc")
set (CMAKE_CXX_COMPILER "${_TRIDENT_CONDA_BIN}/x86_64-conda-linux-gnu-g++")
set (CMAKE_GENERATOR "Unix Makefiles")
set (CMAKE_MAKE_PROGRAM "${_TRIDENT_CONDA_BIN}/make" CACHE FILEPATH "Make program" FORCE)
set (CMAKE_C_COMPILER_FORCED "TRUE")
set (CMAKE_CXX_COMPILER_FORCED "TRUE")
endif()

cmake_minimum_required(VERSION 3.20)
project(TridentTools LANGUAGES C CXX)

include(ExternalProject)

if(WIN32)
    set(_TRIDENT_CMAKE "${_TRIDENT_CONDA_BIN}/cmake.exe")
    if(EXISTS "${_TRIDENT_CONDA_BIN}/git.exe")
        set(_TRIDENT_GIT "${_TRIDENT_CONDA_BIN}/git.exe")
    else()
        set(_TRIDENT_GIT "git")
    endif()
else()
    set(_TRIDENT_CMAKE "${_TRIDENT_CONDA_BIN}/cmake")
    if(EXISTS "${_TRIDENT_CONDA_BIN}/git")
        set(_TRIDENT_GIT "${_TRIDENT_CONDA_BIN}/git")
    else()
        set(_TRIDENT_GIT "git")
    endif()
endif()

set(GIT_EXECUTABLE "${_TRIDENT_GIT}" CACHE FILEPATH "Git executable" FORCE)
set(TOOLS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Tool source directory" FORCE)
set(TOOLS_BUILD_DIR "${CMAKE_BINARY_DIR}/tools/build-current" CACHE PATH "Tool build directory" FORCE)
set(TOOLS_INSTALL_DIR "${CMAKE_BINARY_DIR}/tools/install-current" CACHE PATH "Tool install directory" FORCE)

set(TRIDENT_CONDA_PREFIX "${_TRIDENT_CONDA_ROOT}")
set(TRIDENT_TOOL_ENV
    "PATH=${_TRIDENT_CONDA_BIN}:${_TRIDENT_CONDA_ROOT}/bin:${_TRIDENT_CONDA_ROOT}/Library/usr/bin:$ENV{PATH}"
    "PKG_CONFIG_PATH=${TRIDENT_CONDA_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}"
)

set(TRIDENT_COMMON_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_PREFIX_PATH=${TRIDENT_CONDA_PREFIX}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_LINKER=${CMAKE_LINKER}
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DCMAKE_C_COMPILER_FORCED=${CMAKE_C_COMPILER_FORCED}
    -DCMAKE_CXX_COMPILER_FORCED=${CMAKE_CXX_COMPILER_FORCED}
)

set(TRIDENT_VERSIONS_FILE "${CMAKE_CURRENT_LIST_DIR}/versions" CACHE FILEPATH "Pinned tool versions" FORCE)
if(NOT EXISTS "${TRIDENT_VERSIONS_FILE}")
    message(FATAL_ERROR "Missing tool versions file: ${TRIDENT_VERSIONS_FILE}")
endif()

file(STRINGS "${TRIDENT_VERSIONS_FILE}" TRIDENT_VERSION_LINES)
foreach(version_line IN LISTS TRIDENT_VERSION_LINES)
    string(STRIP "${version_line}" version_line)
    if(version_line STREQUAL "" OR version_line MATCHES "^#")
        continue()
    endif()
    string(REGEX MATCH "^([^ \t]+)[ \t]+([^ \t#]+)" version_match "${version_line}")
    if(NOT version_match)
        message(FATAL_ERROR "Invalid tools/versions line: ${version_line}")
    endif()
    string(MAKE_C_IDENTIFIER "${CMAKE_MATCH_1}" version_key)
    set("TRIDENT_TOOL_VERSION_${version_key}" "${CMAKE_MATCH_2}")
endforeach()

function(trident_tool_version out_var tool_name)
    string(MAKE_C_IDENTIFIER "${tool_name}" tool_key)
    if(NOT DEFINED TRIDENT_TOOL_VERSION_${tool_key})
        message(FATAL_ERROR "Missing ${tool_name} pin in ${TRIDENT_VERSIONS_FILE}")
    endif()
    set(${out_var} "${TRIDENT_TOOL_VERSION_${tool_key}}" PARENT_SCOPE)
endfunction()

function(trident_source_args out_var tool_name tool_url)
    trident_tool_version(tool_version "${tool_name}")
    set(tool_source "${TOOLS_SOURCE_DIR}/${tool_name}")
    if(EXISTS "${tool_source}/.git")
        set(${out_var}
            SOURCE_DIR "${tool_source}"
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND "${GIT_EXECUTABLE}" -C "${tool_source}" checkout --detach "${tool_version}"
            PARENT_SCOPE
        )
    else()
        set(${out_var}
            GIT_REPOSITORY "${tool_url}"
            GIT_TAG "${tool_version}"
            GIT_PROGRESS TRUE
            SOURCE_DIR "${tool_source}"
            PARENT_SCOPE
        )
    endif()
endfunction()

function(trident_cmake_tool tool_name tool_url)
    trident_source_args(source_args "${tool_name}" "${tool_url}")
    if(tool_name STREQUAL "cpphdl")
        set(tool_build_command
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            "${_TRIDENT_CMAKE}" --build <BINARY_DIR> --target cpphdl hdlcpp
            COMMAND "${_TRIDENT_CMAKE}" -E remove_directory "${CMAKE_BINARY_DIR}/cpphdl/include"
            COMMAND "${_TRIDENT_CMAKE}" -E make_directory "${CMAKE_BINARY_DIR}/cpphdl"
            COMMAND "${_TRIDENT_CMAKE}" -E copy_directory
                "${TOOLS_SOURCE_DIR}/cpphdl/include"
                "${CMAKE_BINARY_DIR}/cpphdl/include"
        )
    elseif(tool_name STREQUAL "slang")
        set(tool_build_command
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            "${_TRIDENT_CMAKE}" --build <BINARY_DIR> --parallel 1
        )
    else()
        set(tool_build_command
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            "${_TRIDENT_CMAKE}" --build <BINARY_DIR>
        )
    endif()
    ExternalProject_Add(${tool_name}
        ${source_args}
        EXCLUDE_FROM_ALL TRUE
        BINARY_DIR "${TOOLS_BUILD_DIR}/${tool_name}"
        INSTALL_DIR "${TOOLS_INSTALL_DIR}/${tool_name}"
        CMAKE_COMMAND "${_TRIDENT_CMAKE}"
        CMAKE_GENERATOR "Unix Makefiles"
        CMAKE_ARGS
            ${TRIDENT_COMMON_CMAKE_ARGS}
            ${ARGN}
        CONFIGURE_COMMAND
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            "${_TRIDENT_CMAKE}"
            -G "Unix Makefiles"
            ${TRIDENT_COMMON_CMAKE_ARGS}
            ${ARGN}
            -S <SOURCE_DIR>
            -B <BINARY_DIR>
        BUILD_COMMAND
            ${tool_build_command}
        INSTALL_COMMAND ""
        STEP_TARGETS download update configure build
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_UPDATE TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
    )
endfunction()

trident_tool_version(CPPHDL_SLANG_VERSION slang)
ExternalProject_Add(cpphdl_slang
    GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
    GIT_TAG "${CPPHDL_SLANG_VERSION}"
    GIT_PROGRESS TRUE
    EXCLUDE_FROM_ALL TRUE
    SOURCE_DIR "${TOOLS_SOURCE_DIR}/cpphdl/hdlcpp/slang"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    STEP_TARGETS download
    USES_TERMINAL_DOWNLOAD TRUE
)

trident_cmake_tool(cpphdl https://github.com/mirekez/cpphdl.git
    -DCPPHDL_USE_LOCAL_CONDA=ON
    -DCPPHDL_FETCH_LLVM=OFF
    -DCPPHDL_BUILD_HDLCPP=ON
)
ExternalProject_Add_StepDependencies(cpphdl configure cpphdl_slang)

trident_cmake_tool(scalepnr https://github.com/mirekez/scalepnr.git)

trident_tool_version(CMAKE_HOST_VERSION cmake)
string(REGEX REPLACE "^v" "" CMAKE_HOST_VERSION_NUMBER "${CMAKE_HOST_VERSION}")

if(CMAKE_VERSION VERSION_LESS 3.27)
    ExternalProject_Add(cmake_host
        URL "https://github.com/Kitware/CMake/releases/download/${CMAKE_HOST_VERSION}/cmake-${CMAKE_HOST_VERSION_NUMBER}.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL TRUE
        SOURCE_DIR "${TOOLS_SOURCE_DIR}/cmake-${CMAKE_HOST_VERSION_NUMBER}"
        BINARY_DIR "${TOOLS_BUILD_DIR}/cmake_host"
        INSTALL_DIR "${TOOLS_INSTALL_DIR}/cmake_host"
        CONFIGURE_COMMAND
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            <SOURCE_DIR>/bootstrap --prefix=<INSTALL_DIR> --parallel=1
        BUILD_COMMAND
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            "${CMAKE_MAKE_PROGRAM}" -C <BINARY_DIR> -j1
        INSTALL_COMMAND
            "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
            "${CMAKE_MAKE_PROGRAM}" -C <BINARY_DIR> -j1 install
        STEP_TARGETS download configure build install
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
        USES_TERMINAL_INSTALL TRUE
    )
    set(YOSYS_CMAKE_COMMAND "${TOOLS_INSTALL_DIR}/cmake_host/bin/cmake")
    set(YOSYS_EXTRA_DEPENDS cmake_host)
else()
    set(YOSYS_CMAKE_COMMAND "${_TRIDENT_CMAKE}")
    set(YOSYS_EXTRA_DEPENDS)
endif()

trident_source_args(yosys_source_args yosys https://github.com/YosysHQ/yosys.git)
ExternalProject_Add(yosys
    ${yosys_source_args}
    GIT_SUBMODULES abc libs/cxxopts
    GIT_SUBMODULES_RECURSE TRUE
    EXCLUDE_FROM_ALL TRUE
    DEPENDS ${YOSYS_EXTRA_DEPENDS}
    BINARY_DIR "${TOOLS_BUILD_DIR}/yosys"
    INSTALL_DIR "${TOOLS_INSTALL_DIR}/yosys"
    CMAKE_COMMAND "${YOSYS_CMAKE_COMMAND}"
    CMAKE_GENERATOR "Unix Makefiles"
    CONFIGURE_COMMAND
        "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
        "${YOSYS_CMAKE_COMMAND}"
        -G "Unix Makefiles"
        ${TRIDENT_COMMON_CMAKE_ARGS}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=OFF
        -DYOSYS_WITH_PYTHON=OFF
        -DYOSYS_WITHOUT_READLINE=OFF
        -DYOSYS_WITHOUT_TCL=OFF
        -S <SOURCE_DIR>
        -B <BINARY_DIR>
    BUILD_COMMAND
        "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
        "${YOSYS_CMAKE_COMMAND}" --build <BINARY_DIR>
    INSTALL_COMMAND ""
    STEP_TARGETS download update configure build
    USES_TERMINAL_DOWNLOAD TRUE
    USES_TERMINAL_UPDATE TRUE
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)

trident_tool_version(YOSYS_SLANG_VERSION yosys-slang)
set(YOSYS_SLANG_SOURCE_DIR "${TOOLS_SOURCE_DIR}/yosys-slang")
if(EXISTS "${YOSYS_SLANG_SOURCE_DIR}/.git")
    set(yosys_slang_source_args
        SOURCE_DIR "${YOSYS_SLANG_SOURCE_DIR}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND
            "${GIT_EXECUTABLE}" -C "${YOSYS_SLANG_SOURCE_DIR}" checkout --detach "${YOSYS_SLANG_VERSION}"
            COMMAND "${GIT_EXECUTABLE}" -C "${YOSYS_SLANG_SOURCE_DIR}" submodule update --init third_party/slang third_party/fmt
    )
else()
    set(yosys_slang_source_args
        GIT_REPOSITORY https://github.com/povik/yosys-slang.git
        GIT_TAG "${YOSYS_SLANG_VERSION}"
        GIT_PROGRESS TRUE
        GIT_SUBMODULES third_party/slang third_party/fmt
        GIT_SUBMODULES_RECURSE TRUE
        SOURCE_DIR "${YOSYS_SLANG_SOURCE_DIR}"
    )
endif()

ExternalProject_Add(yosys_slang
    ${yosys_slang_source_args}
    EXCLUDE_FROM_ALL TRUE
    DEPENDS yosys
    BINARY_DIR "${TOOLS_BUILD_DIR}/yosys-slang"
    INSTALL_DIR "${TOOLS_INSTALL_DIR}/yosys-slang"
    CMAKE_COMMAND "${_TRIDENT_CMAKE}"
    CMAKE_GENERATOR "Unix Makefiles"
    CMAKE_ARGS
        ${TRIDENT_COMMON_CMAKE_ARGS}
        -DBUILD_AS_PLUGIN=ON
        -DYOSYS_CONFIG=${TOOLS_BUILD_DIR}/yosys/yosys-config
    CONFIGURE_COMMAND
        "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
        "${_TRIDENT_CMAKE}"
        -G "Unix Makefiles"
        ${TRIDENT_COMMON_CMAKE_ARGS}
        -DBUILD_AS_PLUGIN=ON
        -DYOSYS_CONFIG=${TOOLS_BUILD_DIR}/yosys/yosys-config
        -S <SOURCE_DIR>
        -B <BINARY_DIR>
    BUILD_COMMAND
        "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV}
        "${_TRIDENT_CMAKE}" --build <BINARY_DIR> --target yosys-slang --parallel 1
        COMMAND "${_TRIDENT_CMAKE}" -E make_directory "${TOOLS_BUILD_DIR}/yosys/share/plugins"
        COMMAND "${_TRIDENT_CMAKE}" -E copy_if_different
            <BINARY_DIR>/slang.so
            "${TOOLS_BUILD_DIR}/yosys/share/plugins/slang.so"
    INSTALL_COMMAND ""
    STEP_TARGETS download update configure build
    USES_TERMINAL_DOWNLOAD TRUE
    USES_TERMINAL_UPDATE TRUE
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)

add_custom_target(trident_tools ALL DEPENDS cpphdl scalepnr yosys yosys_slang)
