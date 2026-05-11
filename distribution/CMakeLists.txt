#-----------------------------------------------------------------------------------------------------------------------|
# MIT License (MIT)                                                                                                      |
# Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                            |
#                                                                                                                        |
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          |
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
# the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      |
# to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
#                                                                                                                        |
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
# the Software.                                                                                                          |
#                                                                                                                        |
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
#------------------------------------------------------------------------------------------------------------------------|

cmake_minimum_required(VERSION 3.25.1 FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(distribution CXX C)

# Skip mobile platforms — no dynamic library support / not applicable
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    return()
endif()

# --------------------------------------------------------------------------
# Source / include paths
# --------------------------------------------------------------------------
set(DIST_SRC       ${CMAKE_SOURCE_DIR}/distribution/src)
set(DIST_INC       ${CMAKE_SOURCE_DIR}/distribution/include)
set(SQLITE_SRC     ${CMAKE_SOURCE_DIR}/third-party/lsqlite3)
set(PLUSAES_INC    ${CMAKE_SOURCE_DIR}/third-party)

# --------------------------------------------------------------------------
# Shared library: distribution (.so / .dll / .dylib)
# --------------------------------------------------------------------------
set(CMAKE_SHARED_LIBRARY_PREFIX "")   # no "lib" prefix, mirrors plugin convention

add_library(distribution SHARED
    ${DIST_SRC}/distribution.cpp
    ${DIST_SRC}/sha256.c
    ${SQLITE_SRC}/sqlite3.c
)

target_include_directories(distribution
    PUBLIC  ${DIST_INC}
    PRIVATE ${DIST_SRC}
            ${SQLITE_SRC}
            ${PLUSAES_INC}
)

target_compile_definitions(distribution PRIVATE
    DISTRIBUTION_BUILD_DLL
    SQLITE_CORE
    SQLITE_THREADSAFE=1
)

# MSVC: add dirent shim include path
if (MSVC)
    target_include_directories(distribution PRIVATE
        ${CMAKE_SOURCE_DIR}/include/dirent-1-13
    )
endif()

# Threads and dl (POSIX only)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    find_package(Threads REQUIRED)
    target_link_libraries(distribution PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

# Outputs inherit the global CMAKE_RUNTIME_OUTPUT_DIRECTORY / CMAKE_LIBRARY_OUTPUT_DIRECTORY
# set by the root CMakeLists (FOLDER_BIN/FOLDER_ARCHITECTURE). No override needed.

# --------------------------------------------------------------------------
# CLI executable: distribution (tool)
# --------------------------------------------------------------------------
add_executable(distribution_exe ${DIST_SRC}/main.cpp)

set_target_properties(distribution_exe PROPERTIES
    OUTPUT_NAME distribution
)

target_include_directories(distribution_exe PRIVATE ${DIST_INC})
target_link_libraries(distribution_exe PRIVATE distribution)
