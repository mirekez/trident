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
set (TRIDENT_CPPHDL_LLVM_PREFIX "${_TRIDENT_CONDA_ROOT}/Library" CACHE PATH "CppHDL LLVM/Clang prefix" FORCE)
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
set (TRIDENT_CPPHDL_LLVM_PREFIX "${_TRIDENT_CONDA_ROOT}" CACHE PATH "CppHDL LLVM/Clang prefix" FORCE)
set (CMAKE_GENERATOR "Unix Makefiles")
set (CMAKE_MAKE_PROGRAM "${_TRIDENT_CONDA_BIN}/make" CACHE FILEPATH "Make program" FORCE)
set (CMAKE_C_COMPILER_FORCED "TRUE")
set (CMAKE_CXX_COMPILER_FORCED "TRUE")
endif()

set(TRIDENT_CPPHDL_CLANG_TOOLING_INCLUDE_DIR "")
foreach(_TRIDENT_CPPHDL_INCLUDE_CANDIDATE
    "${TRIDENT_CPPHDL_LLVM_PREFIX}/include"
    "${_TRIDENT_CONDA_ROOT}/Library/include"
    "${_TRIDENT_CONDA_ROOT}/include"
)
    if(EXISTS "${_TRIDENT_CPPHDL_INCLUDE_CANDIDATE}/clang/Tooling/Tooling.h")
        set(TRIDENT_CPPHDL_CLANG_TOOLING_INCLUDE_DIR "${_TRIDENT_CPPHDL_INCLUDE_CANDIDATE}")
        break()
    endif()
endforeach()
if(TRIDENT_CPPHDL_CLANG_TOOLING_INCLUDE_DIR)
    get_filename_component(TRIDENT_CPPHDL_LLVM_PREFIX
        "${TRIDENT_CPPHDL_CLANG_TOOLING_INCLUDE_DIR}" DIRECTORY)
    file(TO_CMAKE_PATH "${TRIDENT_CPPHDL_LLVM_PREFIX}" TRIDENT_CPPHDL_LLVM_PREFIX)
    message(STATUS "CppHDL LLVM/Clang prefix: ${TRIDENT_CPPHDL_LLVM_PREFIX}")
    message(STATUS "CppHDL Clang Tooling include: ${TRIDENT_CPPHDL_CLANG_TOOLING_INCLUDE_DIR}")
else()
    message(WARNING
        "CppHDL did not find clang/Tooling/Tooling.h in local conda. "
        "Checked ${_TRIDENT_CONDA_ROOT}/Library/include and ${_TRIDENT_CONDA_ROOT}/include.")
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
    "CFLAGS="
    "CXXFLAGS="
    "CPPFLAGS="
    "LDFLAGS="
    "OBJCFLAGS="
    "OBJCXXFLAGS="
)

set(TRIDENT_COMMON_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_PREFIX_PATH=${TRIDENT_CONDA_PREFIX}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_LINKER=${CMAKE_LINKER}
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
)

set(TRIDENT_CPPHDL_EXTRA_CMAKE_ARGS
    -DCMAKE_PREFIX_PATH=${TRIDENT_CPPHDL_LLVM_PREFIX}
    -DCPPHDL_USE_LOCAL_CONDA=ON
    -DCPPHDL_LOCAL_CONDA_PREFIX=${TRIDENT_CPPHDL_LLVM_PREFIX}
    -DCPPHDL_FETCH_LLVM=OFF
    -DCPPHDL_BUILD_HDLCPP=ON
)
foreach(_TRIDENT_CPPHDL_PACKAGE_PREFIX
    "${TRIDENT_CPPHDL_LLVM_PREFIX}"
    "${_TRIDENT_CONDA_ROOT}/Library"
    "${_TRIDENT_CONDA_ROOT}"
)
    if(NOT TRIDENT_CPPHDL_LLVM_DIR AND
            EXISTS "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/lib/cmake/llvm/LLVMConfig.cmake")
        set(TRIDENT_CPPHDL_LLVM_DIR "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/lib/cmake/llvm")
    endif()
    if(NOT TRIDENT_CPPHDL_CLANG_DIR AND
            EXISTS "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/lib/cmake/clang/ClangConfig.cmake")
        set(TRIDENT_CPPHDL_CLANG_DIR "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/lib/cmake/clang")
    endif()
    if(NOT TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR AND
            EXISTS "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/include/zlib.h")
        set(TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/include")
        set(TRIDENT_CPPHDL_ZLIB_ROOT "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}")
    endif()
    if(NOT TRIDENT_CPPHDL_ZLIB_LIBRARY)
        foreach(_TRIDENT_CPPHDL_ZLIB_NAME zlib z zlibstatic libzlib libz)
            foreach(_TRIDENT_CPPHDL_ZLIB_SUFFIX lib a dll.a)
                if(EXISTS "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/lib/${_TRIDENT_CPPHDL_ZLIB_NAME}.${_TRIDENT_CPPHDL_ZLIB_SUFFIX}")
                    set(TRIDENT_CPPHDL_ZLIB_LIBRARY
                        "${_TRIDENT_CPPHDL_PACKAGE_PREFIX}/lib/${_TRIDENT_CPPHDL_ZLIB_NAME}.${_TRIDENT_CPPHDL_ZLIB_SUFFIX}")
                    break()
                endif()
            endforeach()
            if(TRIDENT_CPPHDL_ZLIB_LIBRARY)
                break()
            endif()
        endforeach()
    endif()
endforeach()
if(TRIDENT_CPPHDL_LLVM_DIR)
    list(APPEND TRIDENT_CPPHDL_EXTRA_CMAKE_ARGS -DLLVM_DIR=${TRIDENT_CPPHDL_LLVM_DIR})
    message(STATUS "CppHDL LLVM_DIR: ${TRIDENT_CPPHDL_LLVM_DIR}")
endif()
if(TRIDENT_CPPHDL_CLANG_DIR)
    list(APPEND TRIDENT_CPPHDL_EXTRA_CMAKE_ARGS -DClang_DIR=${TRIDENT_CPPHDL_CLANG_DIR})
    message(STATUS "CppHDL Clang_DIR: ${TRIDENT_CPPHDL_CLANG_DIR}")
endif()
if(TRIDENT_CPPHDL_ZLIB_LIBRARY)
    file(TO_CMAKE_PATH "${TRIDENT_CPPHDL_ZLIB_LIBRARY}" TRIDENT_CPPHDL_ZLIB_LIBRARY)
    file(TO_CMAKE_PATH "${TRIDENT_CPPHDL_ZLIB_ROOT}" TRIDENT_CPPHDL_ZLIB_ROOT)
    list(APPEND TRIDENT_CPPHDL_EXTRA_CMAKE_ARGS
        -DZLIB_ROOT=${TRIDENT_CPPHDL_ZLIB_ROOT}
        -DZLIB_LIBRARY=${TRIDENT_CPPHDL_ZLIB_LIBRARY}
        -DTRIDENT_ZLIB_LIBRARY=${TRIDENT_CPPHDL_ZLIB_LIBRARY}
    )
    if(TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR)
        file(TO_CMAKE_PATH "${TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR}" TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR)
        list(APPEND TRIDENT_CPPHDL_EXTRA_CMAKE_ARGS
            -DZLIB_INCLUDE_DIR=${TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR}
            -DTRIDENT_ZLIB_INCLUDE_DIR=${TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR}
        )
    endif()
    message(STATUS "CppHDL ZLIB_LIBRARY: ${TRIDENT_CPPHDL_ZLIB_LIBRARY}")
    if(TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR)
        message(STATUS "CppHDL ZLIB_INCLUDE_DIR: ${TRIDENT_CPPHDL_ZLIB_INCLUDE_DIR}")
    endif()
else()
    message(WARNING
        "CppHDL did not find zlib library in local conda; "
        "LLVMConfig may fail if it requires ZLIB::ZLIB.")
endif()

if(WIN32)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/RunCMakeCommand.sh"
"#!/usr/bin/env sh
set -eu

cmake_cmd=\"\$1\"
shift

to_unix_path() {
    path_value=\"\$1\"
    case \"\$path_value\" in
        ?:/*)
            drive=\"\$(printf '%s' \"\${path_value%%:*}\" | tr 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz')\"
            rest=\"\${path_value#?:}\"
            printf '/%s%s\n' \"\$drive\" \"\$rest\"
            return 0
            ;;
    esac
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u \"\$path_value\" 2>/dev/null || printf '%s\n' \"\$path_value\"
    else
        printf '%s\n' \"\$path_value\"
    fi
}

prepend_path() {
    path_item=\"\$(to_unix_path \"\$1\")\"
    if [ -n \"\$path_item\" ]; then
        PATH=\"\$path_item:\$PATH\"
    fi
}

prepend_path \"${_TRIDENT_CONDA_BIN}\"
prepend_path \"${_TRIDENT_CONDA_ROOT}/bin\"
prepend_path \"${_TRIDENT_CONDA_ROOT}/Library/bin\"
prepend_path \"${_TRIDENT_CONDA_ROOT}/Library/usr/bin\"
export PATH

unset CFLAGS
unset CXXFLAGS
unset CPPFLAGS
unset LDFLAGS
unset OBJCFLAGS
unset OBJCXXFLAGS

cmake_cmd_unix=\"\$(to_unix_path \"\$cmake_cmd\")\"

echo \"Trident CMake wrapper\"
echo \"  requested: \$cmake_cmd\"
echo \"  resolved:  \$cmake_cmd_unix\"

if [ ! -f \"\$cmake_cmd_unix\" ]; then
    echo \"Missing CMake executable: \$cmake_cmd_unix\" >&2
    if command -v ls >/dev/null 2>&1; then
        ls -la \"\$(dirname \"\$cmake_cmd_unix\")\" >&2 || true
    fi
    exit 127
fi

\"\$cmake_cmd_unix\" --version

echo \"  arguments:\"
for arg in \"\$@\"; do
    echo \"    \$arg\"
done

build_dir=\"\"
source_dir=\"\"
previous_arg=\"\"
for arg in \"\$@\"; do
    if [ \"\$previous_arg\" = \"-B\" ]; then
        build_dir=\"\$arg\"
        previous_arg=\"\"
        continue
    fi
    if [ \"\$previous_arg\" = \"-S\" ]; then
        source_dir=\"\$arg\"
        previous_arg=\"\"
        continue
    fi
    case \"\$arg\" in
        -B)
            previous_arg=\"-B\"
            ;;
        -S)
            previous_arg=\"-S\"
            ;;
        -B*)
            build_dir=\"\${arg#-B}\"
            ;;
        -S*)
            source_dir=\"\${arg#-S}\"
            ;;
    esac
done

if [ -n \"\$build_dir\" ] && [ -n \"\$source_dir\" ]; then
    build_dir_unix=\"\$(to_unix_path \"\$build_dir\")\"
    echo \"  clearing:  \$build_dir_unix/CMakeCache.txt\"
    rm -f \"\$build_dir_unix/CMakeCache.txt\"
    rm -rf \"\$build_dir_unix/CMakeFiles\"
fi

exec \"\$cmake_cmd_unix\" \"\$@\"
")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/RunCpphdlPatch.sh" [=[#!/usr/bin/env sh
set -eu

to_unix_path() {
    path_value="$1"
    case "$path_value" in
        ?:/*)
            drive="$(printf '%s' "${path_value%%:*}" | tr 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz')"
            rest="${path_value#?:}"
            printf '/%s%s
' "$drive" "$rest"
            return 0
            ;;
    esac
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$path_value" 2>/dev/null || printf '%s
' "$path_value"
    else
        printf '%s
' "$path_value"
    fi
}

source_dir="$(to_unix_path "${1:-}")"
if [ -z "$source_dir" ]; then
    echo "CPPHDL source directory argument is required" >&2
    exit 2
fi

cmake_file="$source_dir/CMakeLists.txt"
echo "Patching cpphdl explicit LLVM_DIR/Clang_DIR support"
echo "  source: $source_dir"

if [ ! -f "$cmake_file" ]; then
    echo "Missing $cmake_file" >&2
    exit 2
fi

if grep -q 'CPPHDL_EXPLICIT_LLVM_DIR_ZLIB_DIA_PATCH' "$cmake_file"; then
    echo "  already patched"
    exit 0
fi

tmp_file="$cmake_file.tmp.$$"
awk '
function print_explicit_block() {
    print "if(LLVM_DIR AND Clang_DIR)"
    print "  # CPPHDL_EXPLICIT_LLVM_DIR_ZLIB_DIA_PATCH: conda packages may provide CMake configs"
    print "  # even when clang/Tooling headers are not under the probed prefix."
    print "  if(NOT TARGET ZLIB::ZLIB)"
    print "    if(TRIDENT_ZLIB_LIBRARY)"
    print "      add_library(ZLIB::ZLIB UNKNOWN IMPORTED)"
    print "      set_target_properties(ZLIB::ZLIB PROPERTIES IMPORTED_LOCATION \"${TRIDENT_ZLIB_LIBRARY}\")"
    print "      if(TRIDENT_ZLIB_INCLUDE_DIR)"
    print "        set_target_properties(ZLIB::ZLIB PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${TRIDENT_ZLIB_INCLUDE_DIR}\")"
    print "      endif()"
    print "    else()"
    print "      add_library(ZLIB::ZLIB INTERFACE IMPORTED)"
    print "    endif()"
    print "  endif()"
    print "  message(STATUS \"CppHDL using explicit LLVM_DIR=${LLVM_DIR}\")"
    print "  message(STATUS \"CppHDL using explicit Clang_DIR=${Clang_DIR}\")"
    print "  find_package(LLVM REQUIRED CONFIG)"
    print "  if(TARGET LLVMSupport)"
    print "    get_target_property(_cpphdl_llvm_support_links LLVMSupport INTERFACE_LINK_LIBRARIES)"
    print "    if(_cpphdl_llvm_support_links)"
    print "      list(FILTER _cpphdl_llvm_support_links EXCLUDE REGEX \".*([Dd][Ii][Aa][ /\\\\]SDK|diaguids).*\")"
    print "      set_target_properties(LLVMSupport PROPERTIES INTERFACE_LINK_LIBRARIES \"${_cpphdl_llvm_support_links}\")"
    print "    endif()"
    print "  endif()"
    print "  find_package(Clang REQUIRED CONFIG)"
    print "  include_directories(${LLVM_INCLUDE_DIRS})"
    print "  include_directories(${CLANG_INCLUDE_DIRS})"
    print "elseif(CPPHDL_LLVM_PREFIXES)"
    print "  find_package(LLVM REQUIRED CONFIG PATHS ${CPPHDL_LLVM_PREFIXES} NO_DEFAULT_PATH)"
    print "  find_package(Clang REQUIRED CONFIG PATHS ${CPPHDL_LLVM_PREFIXES} NO_DEFAULT_PATH)"
    print "  include_directories(${LLVM_INCLUDE_DIRS})"
    print "  include_directories(${CLANG_INCLUDE_DIRS})"
}
BEGIN { patched = 0; skipping = 0 }
($0 == "if(CPPHDL_LLVM_PREFIXES)" || $0 == "if(LLVM_DIR AND Clang_DIR)") && patched == 0 {
    print_explicit_block()
    patched = 1
    skipping = 1
    next
}
skipping == 1 {
    if ($0 == "elseif(CPPHDL_FETCH_LLVM)") {
        print
        skipping = 0
    }
    next
}
{ print }
END {
    if (patched == 0) {
        print "Could not locate LLVM find_package block" > "/dev/stderr"
        exit 2
    }
}
' "$cmake_file" > "$tmp_file"
mv "$tmp_file" "$cmake_file"
echo "  patched CMakeLists.txt"
]=])
    set(TRIDENT_CPPHDL_PATCH_COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/RunCpphdlPatch.sh" <SOURCE_DIR>)
    set(TRIDENT_CMAKE_LAUNCHER sh "${CMAKE_CURRENT_BINARY_DIR}/RunCMakeCommand.sh")
else()
    set(TRIDENT_CPPHDL_PATCH_COMMAND sh "${CMAKE_CURRENT_LIST_DIR}/PatchCpphdlUseExplicitLlvm.sh" <SOURCE_DIR>)
    set(TRIDENT_CMAKE_LAUNCHER "${_TRIDENT_CMAKE}" -E env ${TRIDENT_TOOL_ENV})
endif()

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

set(TRIDENT_YOSYS_EXTRA_CMAKE_ARGS)
set(TRIDENT_YOSYS_EXTRA_DEPENDS)
if(WIN32)
    find_program(TRIDENT_FLEX_EXECUTABLE
        NAMES win_flex win_flex.exe flex flex.exe
        PATHS
            "${_TRIDENT_CONDA_ROOT}/Library/bin"
            "${_TRIDENT_CONDA_ROOT}/Library/usr/bin"
            "${_TRIDENT_CONDA_ROOT}/bin"
        ENV PATH
        NO_DEFAULT_PATH
    )
    find_program(TRIDENT_BISON_EXECUTABLE
        NAMES win_bison win_bison.exe bison bison.exe
        PATHS
            "${_TRIDENT_CONDA_ROOT}/Library/bin"
            "${_TRIDENT_CONDA_ROOT}/Library/usr/bin"
            "${_TRIDENT_CONDA_ROOT}/bin"
        ENV PATH
        NO_DEFAULT_PATH
    )

    if(NOT TRIDENT_FLEX_EXECUTABLE OR NOT TRIDENT_BISON_EXECUTABLE)
        trident_tool_version(WINFLEXBISON_VERSION winflexbison)
        string(REGEX REPLACE "^v" "" WINFLEXBISON_VERSION_NUMBER "${WINFLEXBISON_VERSION}")
        ExternalProject_Add(winflexbison
            URL "https://github.com/lexxmark/winflexbison/releases/download/${WINFLEXBISON_VERSION}/win_flex_bison-${WINFLEXBISON_VERSION_NUMBER}.zip"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            EXCLUDE_FROM_ALL TRUE
            SOURCE_DIR "${TOOLS_BUILD_DIR}/winflexbison"
            BINARY_DIR "${TOOLS_BUILD_DIR}/winflexbison-build"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            STEP_TARGETS download
            USES_TERMINAL_DOWNLOAD TRUE
        )
        set(TRIDENT_FLEX_EXECUTABLE "${TOOLS_BUILD_DIR}/winflexbison/win_flex.exe")
        set(TRIDENT_BISON_EXECUTABLE "${TOOLS_BUILD_DIR}/winflexbison/win_bison.exe")
        list(APPEND TRIDENT_YOSYS_EXTRA_DEPENDS winflexbison)
        message(STATUS "Using downloaded winflexbison for Yosys")
    endif()

    file(TO_CMAKE_PATH "${TRIDENT_FLEX_EXECUTABLE}" TRIDENT_FLEX_EXECUTABLE)
    file(TO_CMAKE_PATH "${TRIDENT_BISON_EXECUTABLE}" TRIDENT_BISON_EXECUTABLE)
    list(APPEND TRIDENT_YOSYS_EXTRA_CMAKE_ARGS
        -DFLEX_EXECUTABLE=${TRIDENT_FLEX_EXECUTABLE}
        -DBISON_EXECUTABLE=${TRIDENT_BISON_EXECUTABLE}
    )
    message(STATUS "Using FLEX for Yosys: ${TRIDENT_FLEX_EXECUTABLE}")
    message(STATUS "Using BISON for Yosys: ${TRIDENT_BISON_EXECUTABLE}")
endif()

function(trident_cmake_tool tool_name tool_url)
    trident_source_args(source_args "${tool_name}" "${tool_url}")
    set(tool_patch_command "")
    if(tool_name STREQUAL "cpphdl")
        set(tool_patch_command
            ${TRIDENT_CPPHDL_PATCH_COMMAND}
        )
        set(tool_build_command
            ${TRIDENT_CMAKE_LAUNCHER}
            "${_TRIDENT_CMAKE}" --build <BINARY_DIR> --target cpphdl hdlcpp
            COMMAND "${_TRIDENT_CMAKE}" -E remove_directory "${CMAKE_BINARY_DIR}/cpphdl/include"
            COMMAND "${_TRIDENT_CMAKE}" -E make_directory "${CMAKE_BINARY_DIR}/cpphdl"
            COMMAND "${_TRIDENT_CMAKE}" -E copy_directory
                "${TOOLS_SOURCE_DIR}/cpphdl/include"
                "${CMAKE_BINARY_DIR}/cpphdl/include"
        )
    elseif(tool_name STREQUAL "slang")
        set(tool_build_command
            ${TRIDENT_CMAKE_LAUNCHER}
            "${_TRIDENT_CMAKE}" --build <BINARY_DIR> --parallel 1
        )
    else()
        set(tool_build_command
            ${TRIDENT_CMAKE_LAUNCHER}
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
        PATCH_COMMAND ${tool_patch_command}
        CMAKE_ARGS
            ${TRIDENT_COMMON_CMAKE_ARGS}
            ${ARGN}
        CONFIGURE_COMMAND
            ${TRIDENT_CMAKE_LAUNCHER}
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
    ${TRIDENT_CPPHDL_EXTRA_CMAKE_ARGS}
)
ExternalProject_Add_StepDependencies(cpphdl configure cpphdl_slang)

trident_cmake_tool(scalepnr https://github.com/mirekez/scalepnr.git)

trident_tool_version(CMAKE_HOST_VERSION cmake)
string(REGEX REPLACE "^v" "" CMAKE_HOST_VERSION_NUMBER "${CMAKE_HOST_VERSION}")

set(YOSYS_PATCH_COMMAND "")
if(WIN32 AND CMAKE_VERSION VERSION_LESS 3.27)
    set(YOSYS_CMAKE_COMMAND "${_TRIDENT_CMAKE}")
    set(YOSYS_EXTRA_DEPENDS)
    set(YOSYS_PATCH_COMMAND
        "${_TRIDENT_CMAKE}"
            -DYOSYS_SOURCE_DIR=<SOURCE_DIR>
            -P "${CMAKE_CURRENT_LIST_DIR}/PatchYosysCMakeForWindows.cmake"
    )
elseif(CMAKE_VERSION VERSION_LESS 3.27)
    set(CMAKE_HOST_URL "https://github.com/Kitware/CMake/releases/download/${CMAKE_HOST_VERSION}/cmake-${CMAKE_HOST_VERSION_NUMBER}-linux-x86_64.tar.gz")
    set(CMAKE_HOST_EXECUTABLE "${TOOLS_INSTALL_DIR}/cmake_host/bin/cmake")
    ExternalProject_Add(cmake_host
        URL "${CMAKE_HOST_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL TRUE
        SOURCE_DIR "${TOOLS_BUILD_DIR}/cmake_host_prebuilt"
        BINARY_DIR "${TOOLS_BUILD_DIR}/cmake_host"
        INSTALL_DIR "${TOOLS_INSTALL_DIR}/cmake_host"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND
            "${_TRIDENT_CMAKE}"
                -DSOURCE_DIR=<SOURCE_DIR>
                -DINSTALL_DIR=<INSTALL_DIR>
                -P "${CMAKE_CURRENT_LIST_DIR}/InstallPrebuiltCMake.cmake"
        STEP_TARGETS download install
        USES_TERMINAL_DOWNLOAD TRUE
        USES_TERMINAL_INSTALL TRUE
    )
    set(YOSYS_CMAKE_COMMAND "${CMAKE_HOST_EXECUTABLE}")
    set(YOSYS_EXTRA_DEPENDS cmake_host)
else()
    set(YOSYS_CMAKE_COMMAND "${_TRIDENT_CMAKE}")
    set(YOSYS_EXTRA_DEPENDS)
endif()

set(TRIDENT_YOSYS_CMAKE_LAUNCHER ${TRIDENT_CMAKE_LAUNCHER} "${YOSYS_CMAKE_COMMAND}")

trident_source_args(yosys_source_args yosys https://github.com/YosysHQ/yosys.git)
ExternalProject_Add(yosys
    ${yosys_source_args}
    GIT_SUBMODULES abc libs/cxxopts
    GIT_SUBMODULES_RECURSE TRUE
    EXCLUDE_FROM_ALL TRUE
    DEPENDS ${YOSYS_EXTRA_DEPENDS} ${TRIDENT_YOSYS_EXTRA_DEPENDS}
    PATCH_COMMAND ${YOSYS_PATCH_COMMAND}
    BINARY_DIR "${TOOLS_BUILD_DIR}/yosys"
    INSTALL_DIR "${TOOLS_INSTALL_DIR}/yosys"
    CMAKE_COMMAND "${YOSYS_CMAKE_COMMAND}"
    CMAKE_GENERATOR "Unix Makefiles"
    CONFIGURE_COMMAND
        ${TRIDENT_YOSYS_CMAKE_LAUNCHER}
        -G "Unix Makefiles"
        ${TRIDENT_COMMON_CMAKE_ARGS}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=OFF
        -DYOSYS_WITH_PYTHON=OFF
        -DYOSYS_WITHOUT_READLINE=OFF
        -DYOSYS_WITHOUT_TCL=OFF
        ${TRIDENT_YOSYS_EXTRA_CMAKE_ARGS}
        -S <SOURCE_DIR>
        -B <BINARY_DIR>
    BUILD_COMMAND
        ${TRIDENT_YOSYS_CMAKE_LAUNCHER} --build <BINARY_DIR>
    INSTALL_COMMAND ""
    STEP_TARGETS download update configure build
    USES_TERMINAL_DOWNLOAD TRUE
    USES_TERMINAL_UPDATE TRUE
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)

if(WIN32)
    ExternalProject_Add_Step(yosys windows_import_alias
        DEPENDEES build
        COMMAND "${_TRIDENT_CMAKE}" -E copy_if_different
            "${TOOLS_BUILD_DIR}/yosys/yosys.exe.a"
            "${TOOLS_BUILD_DIR}/yosys/libyosys_exe.a"
    )
endif()

trident_tool_version(YOSYS_SLANG_VERSION yosys-slang)
set(YOSYS_SLANG_SOURCE_DIR "${TOOLS_SOURCE_DIR}/yosys-slang")
set(TRIDENT_YOSYS_CONFIG_FOR_PLUGIN "${TOOLS_BUILD_DIR}/yosys/yosys-config")
set(YOSYS_SLANG_PATCH_COMMAND "")
if(WIN32)
    find_program(TRIDENT_SH_EXECUTABLE
        NAMES sh sh.exe
        PATHS
            "${_TRIDENT_CONDA_ROOT}/Library/usr/bin"
            "${_TRIDENT_CONDA_ROOT}/usr/bin"
            "${_TRIDENT_CONDA_ROOT}/bin"
        ENV PATH
        NO_DEFAULT_PATH
    )
    if(NOT TRIDENT_SH_EXECUTABLE)
        set(TRIDENT_SH_EXECUTABLE "sh")
    endif()
    file(TO_CMAKE_PATH "${TRIDENT_SH_EXECUTABLE}" TRIDENT_SH_EXECUTABLE)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/RunYosysConfig.cmd"
"@echo off\r
\"${TRIDENT_SH_EXECUTABLE}\" \"${TOOLS_BUILD_DIR}/yosys/yosys-config\" %*\r
")
    set(TRIDENT_YOSYS_CONFIG_FOR_PLUGIN "${CMAKE_CURRENT_BINARY_DIR}/RunYosysConfig.cmd")
endif()
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
    PATCH_COMMAND ${YOSYS_SLANG_PATCH_COMMAND}
    BINARY_DIR "${TOOLS_BUILD_DIR}/yosys-slang"
    INSTALL_DIR "${TOOLS_INSTALL_DIR}/yosys-slang"
    CMAKE_COMMAND "${_TRIDENT_CMAKE}"
    CMAKE_GENERATOR "Unix Makefiles"
    CMAKE_ARGS
        ${TRIDENT_COMMON_CMAKE_ARGS}
        -DBUILD_AS_PLUGIN=ON
        -DYOSYS_CONFIG=${TRIDENT_YOSYS_CONFIG_FOR_PLUGIN}
        "-DCMAKE_C_FLAGS_RELEASE=-O1 -g0 -DNDEBUG"
        "-DCMAKE_CXX_FLAGS_RELEASE=-O1 -g0 -DNDEBUG"
    CONFIGURE_COMMAND
        ${TRIDENT_CMAKE_LAUNCHER}
        "${_TRIDENT_CMAKE}"
        -G "Unix Makefiles"
        ${TRIDENT_COMMON_CMAKE_ARGS}
        -DBUILD_AS_PLUGIN=ON
        -DYOSYS_CONFIG=${TRIDENT_YOSYS_CONFIG_FOR_PLUGIN}
        "-DCMAKE_C_FLAGS_RELEASE=-O1 -g0 -DNDEBUG"
        "-DCMAKE_CXX_FLAGS_RELEASE=-O1 -g0 -DNDEBUG"
        -S <SOURCE_DIR>
        -B <BINARY_DIR>
    BUILD_COMMAND
        ${TRIDENT_CMAKE_LAUNCHER}
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
