cmake_minimum_required(VERSION 3.10)

message(STATUS "Patching yosys-slang for Windows Yosys import library")

if(NOT DEFINED YOSYS_SLANG_SOURCE_DIR)
    message(FATAL_ERROR "YOSYS_SLANG_SOURCE_DIR is required")
endif()

file(TO_CMAKE_PATH "${YOSYS_SLANG_SOURCE_DIR}" yosys_slang_source_dir)
set(find_yosys_cmake "${yosys_slang_source_dir}/cmake/FindYosys.cmake")
if(NOT EXISTS "${find_yosys_cmake}")
    message(FATAL_ERROR "Missing ${find_yosys_cmake}")
endif()

file(READ "${find_yosys_cmake}" find_yosys_content)

string(FIND "${find_yosys_content}" "if(WIN32)" win32_pos)
string(FIND "${find_yosys_content}" "set(YOSYS_BINDIR" suffix_pos)
if(win32_pos EQUAL -1 OR suffix_pos EQUAL -1 OR NOT win32_pos LESS suffix_pos)
    message(FATAL_ERROR "Could not locate Windows link block in ${find_yosys_cmake}")
endif()

string(SUBSTRING "${find_yosys_content}" 0 ${win32_pos} prefix)
string(LENGTH "${find_yosys_content}" content_len)
math(EXPR suffix_len "${content_len} - ${suffix_pos}")
string(SUBSTRING "${find_yosys_content}" ${suffix_pos} ${suffix_len} suffix)

set(new_win32_block "if(WIN32)
    execute_process(
        COMMAND \${YOSYS_CONFIG} --linkflags
        OUTPUT_VARIABLE YOSYS_LINKFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    string(REGEX REPLACE \" +\" \";\" YOSYS_LINKFLAGS \${YOSYS_LINKFLAGS})
    list(FILTER YOSYS_LINKFLAGS INCLUDE REGEX \"^-[L]\")
    message(STATUS \"yosys-config --linkflags (filtered): \${YOSYS_LINKFLAGS}\")

    target_link_options(yosys::yosys INTERFACE \${YOSYS_LINKFLAGS})

    set(YOSYS_EXE_IMPORT_LIBRARY \"\${YOSYS_BINDIR}/yosys.exe.a\")
    if(EXISTS \"\${YOSYS_EXE_IMPORT_LIBRARY}\")
        target_link_libraries(yosys::yosys INTERFACE \"\${YOSYS_EXE_IMPORT_LIBRARY}\")
    else()
        target_link_libraries(yosys::yosys INTERFACE yosys_exe)
    endif()
endif()

")

file(WRITE "${find_yosys_cmake}" "${prefix}${new_win32_block}${suffix}")
message(STATUS "  patched FindYosys.cmake")
