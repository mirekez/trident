set (_TRIDENT_CONDA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.conda")
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
project(TridentGuiRpcDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BACKEND_SOURCES
    backend/Server.cpp
    backend/BashConsoleSession.cpp
    backend/CompilationFlow.cpp
    backend/Project.cpp
    backend/RPCStatus.cpp
    backend/RPCMetrics.cpp
    backend/RPCPicture.cpp
    backend/RPCLoadFile.cpp
    backend/RPCGetOpenedFileList.cpp
    backend/RPCRefreshDevLog.cpp
    backend/RPCExecBashCommand.cpp
    backend/RPCRunBashCommand.cpp
    backend/tests/RPCStatusTest.cpp
    backend/tests/RPCMetricsTest.cpp
    backend/tests/RPCPictureTest.cpp
    backend/tests/RPCLoadFileTest.cpp
    backend/tests/RPCGetOpenedFileListTest.cpp
    backend/tests/RPCRefreshDevLogTest.cpp
    backend/tests/RPCExecBashCommandTest.cpp
    backend/tests/RPCRunBashCommandTest.cpp
)

add_library(trident_backend_lib ${BACKEND_SOURCES})
target_include_directories(trident_backend_lib PUBLIC backend backend/tests)

if(WIN32)
    target_link_libraries(trident_backend_lib PUBLIC ws2_32)
else()
    target_link_libraries(trident_backend_lib PUBLIC util)
endif()

add_executable(trident_backend backend/main.cpp)
target_link_libraries(trident_backend PRIVATE trident_backend_lib)

if(NOT WIN32)
    target_link_options(trident_backend PRIVATE -static-libstdc++ -static-libgcc)
endif()

add_custom_command(TARGET trident_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/default_flow.json"
        "$<TARGET_FILE_DIR:trident_backend>/default_flow.json"
)

add_executable(trident_backend_tests backend/tests/main.cpp)
target_link_libraries(trident_backend_tests PRIVATE trident_backend_lib)

if(NOT WIN32)
    target_link_options(trident_backend_tests PRIVATE -static-libstdc++ -static-libgcc)
endif()

add_executable(trident_starter starter.cpp)

if(NOT WIN32)
    target_link_options(trident_starter PRIVATE -static-libstdc++ -static-libgcc)
endif()

enable_testing()
add_test(NAME trident_backend_tests COMMAND trident_backend_tests)

add_custom_target(gui
    COMMAND ${CMAKE_COMMAND} -E echo "GUI is plain JavaScript in ${CMAKE_SOURCE_DIR}/gui and is served by trident_backend."
    SOURCES gui/index.html gui/app.js gui/FocusedControl.js gui/CodeEditor.js gui/EditorTabs.js gui/Development.js gui/Log.js gui/Console.js gui/PathSelector.js gui/OpenFile.js gui/ProjectOptions.js gui/FileSelect.js gui/Filesysten.js gui/styles.css gui/package.json
)

add_subdirectory(tools)
