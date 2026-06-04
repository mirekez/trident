if(WIN32)
set (_TRIDENT_CONDA_BIN "${CMAKE_CURRENT_LIST_DIR}/.conda/bin")
if(NOT EXISTS "${_TRIDENT_CONDA_BIN}")
set (_TRIDENT_CONDA_BIN "$ENV{CONDA_PREFIX}/bin")
endif()
set (CMAKE_LINKER "${_TRIDENT_CONDA_BIN}/ld64.lld.exe")
set (CMAKE_C_COMPILER "${_TRIDENT_CONDA_BIN}/x86_64-w64-mingw32-gcc")
set (CMAKE_CXX_COMPILER "${_TRIDENT_CONDA_BIN}/x86_64-w64-mingw32-g++")
set (CMAKE_GENERATOR "Unix Makefiles")
set (CMAKE_MAKE_PROGRAM "${_TRIDENT_CONDA_BIN}/make" CACHE FILEPATH "Make program" FORCE)
set (CMAKE_C_COMPILER_FORCED "TRUE")
set (CMAKE_CXX_COMPILER_FORCED "TRUE")
else()
set (_TRIDENT_CONDA_BIN "${CMAKE_CURRENT_LIST_DIR}/.conda/bin")
if(NOT EXISTS "${_TRIDENT_CONDA_BIN}")
set (_TRIDENT_CONDA_BIN "$ENV{CONDA_PREFIX}/bin")
endif()
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
    backend/RPCStatus.cpp
    backend/RPCMetrics.cpp
    backend/RPCPicture.cpp
    backend/tests/RPCStatusTest.cpp
    backend/tests/RPCMetricsTest.cpp
    backend/tests/RPCPictureTest.cpp
)

add_library(trident_backend_lib ${BACKEND_SOURCES})
target_include_directories(trident_backend_lib PUBLIC backend backend/tests)

if(WIN32)
    target_link_libraries(trident_backend_lib PUBLIC ws2_32)
endif()

add_executable(trident_backend backend/main.cpp)
target_link_libraries(trident_backend PRIVATE trident_backend_lib)

add_executable(trident_backend_tests backend/tests/main.cpp)
target_link_libraries(trident_backend_tests PRIVATE trident_backend_lib)

add_executable(trident_starter starter.cpp)

enable_testing()
add_test(NAME trident_backend_tests COMMAND trident_backend_tests)

add_custom_target(gui
    COMMAND ${CMAKE_COMMAND} -E echo "GUI is plain JavaScript in ${CMAKE_SOURCE_DIR}/gui and is served by trident_backend."
    SOURCES gui/index.html gui/app.js gui/styles.css gui/package.json
)

add_subdirectory(tools)
