cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED YOSYS_SOURCE_DIR)
    message(FATAL_ERROR "YOSYS_SOURCE_DIR is required")
endif()

# In the pinned Yosys revision, REQUIRES selects components for the final
# executable but does not propagate their compile-time include paths.
# Declare optional libraries on every component that includes their headers.
function(patch_yosys_dependencies relative_path component source libraries)
    set(cmake_file "${YOSYS_SOURCE_DIR}/${relative_path}/CMakeLists.txt")
    file(READ "${cmake_file}" cmake_text)
    set(original "${component}\n\t${source}\n")
    set(patched "${original}\tLIBRARIES\n${libraries}")

    string(FIND "${cmake_text}" "${patched}" patched_pos)
    if(NOT patched_pos EQUAL -1)
        message(STATUS "Yosys dependencies already patched: ${component}")
        return()
    endif()

    string(FIND "${cmake_text}" "${original}" original_pos)
    if(original_pos EQUAL -1)
        message(FATAL_ERROR "Could not locate ${component} in ${cmake_file}")
    endif()
    string(REPLACE "${original}" "${patched}" cmake_text "${cmake_text}")
    file(WRITE "${cmake_file}" "${cmake_text}")
    message(STATUS "Patched Yosys dependencies: ${component}")
endfunction()

set(zlib_dependency "\t\t$<\${YOSYS_ENABLE_ZLIB}:PkgConfig::zlib>\n")
# fstapi.h exposes zlib headers to both of these components.
patch_yosys_dependencies(kernel "yosys_core(fstdata" "fstdata.cc\n\tfstdata.h" "${zlib_dependency}")
patch_yosys_dependencies(passes/sat "yosys_pass(sim" "sim.cc" "${zlib_dependency}")
# These passes include kernel/gzip.h, which also exposes zlib headers.
patch_yosys_dependencies(passes/cmds "yosys_pass(stat" "stat.cc" "${zlib_dependency}")
patch_yosys_dependencies(passes/techmap "yosys_pass(clockgate" "clockgate.cc" "${zlib_dependency}")
patch_yosys_dependencies(passes/techmap "yosys_pass(dfflibmap" "dfflibmap.cc" "${zlib_dependency}")
# Match the optional line-editing dependencies already declared by show.
patch_yosys_dependencies(passes/cmds "yosys_pass(viz" "viz.cc"
    "\t\t$<\${YOSYS_ENABLE_READLINE}:PkgConfig::readline>\n\t\t$<\${YOSYS_ENABLE_EDITLINE}:PkgConfig::editline>\n")
