if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

if(NOT DEFINED INSTALL_DIR)
    message(FATAL_ERROR "INSTALL_DIR is required")
endif()

file(TO_CMAKE_PATH "${SOURCE_DIR}" SOURCE_DIR)
file(TO_CMAKE_PATH "${INSTALL_DIR}" INSTALL_DIR)

message(STATUS "Installing prebuilt CMake")
message(STATUS "  source:  ${SOURCE_DIR}")
message(STATUS "  install: ${INSTALL_DIR}")

file(GLOB cmake_children LIST_DIRECTORIES TRUE "${SOURCE_DIR}/*")
set(cmake_roots "${SOURCE_DIR}" ${cmake_children})

set(cmake_root_dir "")
foreach(candidate_root IN LISTS cmake_roots)
    if(EXISTS "${candidate_root}/bin/cmake" OR EXISTS "${candidate_root}/bin/cmake.exe")
        set(cmake_root_dir "${candidate_root}")
        break()
    endif()
endforeach()

if(cmake_root_dir STREQUAL "")
    message(STATUS "Contents of ${SOURCE_DIR}:")
    foreach(entry IN LISTS cmake_children)
        message(STATUS "  ${entry}")
    endforeach()
    message(FATAL_ERROR "Could not find bin/cmake or bin/cmake.exe below ${SOURCE_DIR}")
endif()

message(STATUS "  selected: ${cmake_root_dir}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${INSTALL_DIR}"
    RESULT_VARIABLE remove_result
    OUTPUT_VARIABLE remove_output
    ERROR_VARIABLE remove_error
)
if(NOT remove_result EQUAL 0)
    message(FATAL_ERROR "Failed to remove ${INSTALL_DIR}: ${remove_error}${remove_output}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${INSTALL_DIR}"
    RESULT_VARIABLE mkdir_result
    OUTPUT_VARIABLE mkdir_output
    ERROR_VARIABLE mkdir_error
)
if(NOT mkdir_result EQUAL 0)
    message(FATAL_ERROR "Failed to create ${INSTALL_DIR}: ${mkdir_error}${mkdir_output}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${cmake_root_dir}" "${INSTALL_DIR}"
    RESULT_VARIABLE copy_result
    OUTPUT_VARIABLE copy_output
    ERROR_VARIABLE copy_error
)
if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy ${cmake_root_dir} to ${INSTALL_DIR}: ${copy_error}${copy_output}")
endif()

if(NOT EXISTS "${INSTALL_DIR}/bin/cmake" AND NOT EXISTS "${INSTALL_DIR}/bin/cmake.exe")
    message(FATAL_ERROR "Installed CMake is missing ${INSTALL_DIR}/bin/cmake")
endif()

message(STATUS "Prebuilt CMake installed")
