# +--------------------------------------------------------------------------+
# | MIT License - Copyright (c) 2026 Michel Braz de Morais                    |
# | Permission is hereby granted, free of charge, to any person obtaining a  |
# | copy of this software and associated documentation files (the Software), |
# | to deal in the Software without restriction, including without limitation|
# | the rights to use, copy, modify, merge, publish, distribute, sublicense,  |
# | and/or sell copies, and to permit persons to whom the Software is        |
# | furnished to do so, subject to the following conditions:                 |
# | The above copyright notice and this permission notice shall be included |
# | in all copies or substantial portions of the Software.                  |
# | THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR |
# | IMPLIED, INCLUDING BUT NOT LIMITED TO MERCHANTABILITY, FITNESS FOR A    |
# | PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS   |
# | OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          |
# | LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING  |
# | FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR ITS USE.              |
# +--------------------------------------------------------------------------+

if(NOT ENGINE_TARGET_PLATFORM STREQUAL "Windows" OR NOT MINGW)
    message(FATAL_ERROR "ENGINE_DISTRIBUTION requires Windows with MinGW.")
endif()
if(DEFINED GAME_ASSETS_DIR)
    message(FATAL_ERROR "ENGINE_DISTRIBUTION cannot be combined with GAME_ASSETS_DIR. Use a separate build directory.")
endif()
foreach(_feature USE_LUA USE_IMGUI USE_LSQLITE3 USE_TILEMAP USE_BOX2D USE_BOX2D_LIQUID_FUN)
    if(NOT ${_feature})
        message(FATAL_ERROR "ENGINE_DISTRIBUTION requires ${_feature}. Configure with -DUSE_ALL=1.")
    endif()
endforeach()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "ENGINE_DISTRIBUTION requires -DCMAKE_BUILD_TYPE=Release.")
endif()

# Enumerate current targets, not the shared bin folder (which can contain stale DLLs).
function(mbm_collect_package_libraries directory output)
    get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_libraries "")
    foreach(_target IN LISTS _targets)
        get_target_property(_type "${_target}" TYPE)
        if(_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "MODULE_LIBRARY")
            list(APPEND _libraries "${_target}")
        endif()
    endforeach()
    get_property(_children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_child IN LISTS _children)
        mbm_collect_package_libraries("${_child}" _child_libraries)
        list(APPEND _libraries ${_child_libraries})
    endforeach()
    set(${output} "${_libraries}" PARENT_SCOPE)
endfunction()

mbm_collect_package_libraries("${CMAKE_SOURCE_DIR}" _package_libraries)
# Reuse the launcher artwork without the Visual Studio-specific resource script.
file(WRITE "${CMAKE_BINARY_DIR}/engine-dev-icon.rc"
    "101 ICON \"${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm-launcher/laucher.ico\"\n")
target_sources(mini-mbm-dev PRIVATE "${CMAKE_BINARY_DIR}/engine-dev-icon.rc")
set(_package_executables mini-mbm-dev mini-mbm distribution_exe)
set(ENGINE_PACKAGE_EXECUTABLES "")
set(ENGINE_PACKAGE_LIBRARIES "")
foreach(_target IN LISTS _package_executables)
    list(APPEND ENGINE_PACKAGE_EXECUTABLES "$<TARGET_FILE:${_target}>")
endforeach()
foreach(_target IN LISTS _package_libraries)
    list(APPEND ENGINE_PACKAGE_LIBRARIES "$<TARGET_FILE:${_target}>")
endforeach()

file(STRINGS "${CMAKE_SOURCE_DIR}/include/version/version.h" _version_line
    REGEX "^[ \t]*#define MBM_VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" ENGINE_PACKAGE_VERSION "${_version_line}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/include/version/version.h")
set(_architecture x86)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_architecture x64)
endif()
set(ENGINE_PACKAGE_BASENAME "mini-mbm-dev-${ENGINE_PACKAGE_VERSION}-windows-${_architecture}")
set(ENGINE_PACKAGE_STAGE "${CMAKE_BINARY_DIR}/mini-mbm-dev.EngineDir")
get_filename_component(ENGINE_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(ENGINE_SYSTEM_DIR "$ENV{SystemRoot}/System32")
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND EXISTS "$ENV{SystemRoot}/SysWOW64")
    set(ENGINE_SYSTEM_DIR "$ENV{SystemRoot}/SysWOW64")
endif()
file(TO_CMAKE_PATH "${ENGINE_SYSTEM_DIR}" ENGINE_SYSTEM_DIR)
set(ENGINE_EXTERNAL_LIBRARIES "${ENGINE_SYSTEM_DIR}/D3DCompiler_47.dll")
if(USE_STEAM)
    file(TO_CMAKE_PATH "${STEAMWORKS_SDK_PATH}" _steam_sdk)
    set(_steam_runtime "${_steam_sdk}/redistributable_bin/steam_api.dll")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_steam_runtime "${_steam_sdk}/redistributable_bin/win64/steam_api64.dll")
    endif()
    if(NOT EXISTS "${_steam_runtime}")
        message(FATAL_ERROR "Steam runtime required for engine distribution: ${_steam_runtime}")
    endif()
    # steam.dll is a build target; the SDK runtime is not. Stage both before
    # inspecting imports, without relying on a previous copy in the bin folder.
    list(APPEND ENGINE_EXTERNAL_LIBRARIES "${_steam_runtime}")
endif()
if(AUDIO STREQUAL "portaudio")
    list(APPEND ENGINE_EXTERNAL_LIBRARIES "${CMAKE_SOURCE_DIR}/third-party/portaudio/Windows/bin/portaudio_x86.dll")
endif()
if(NOT USE_DIRECTX9 AND NOT USE_DIRECTX11)
    list(APPEND ENGINE_EXTERNAL_LIBRARIES
        "${CMAKE_SOURCE_DIR}/third-party/gles/bin/libEGL.dll" "${CMAKE_SOURCE_DIR}/third-party/gles/bin/libGLESv2.dll")
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/stage-engine.cmake.in"
    "${CMAKE_BINARY_DIR}/stage-engine.cmake.gen" @ONLY)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/stage-engine.cmake"
    INPUT "${CMAKE_BINARY_DIR}/stage-engine.cmake.gen")
# NSIS !include needs backslashes even when MinGW CMake regards '/' as native.
string(REPLACE "/" "\\" ENGINE_UNINSTALL_MANIFEST "${CMAKE_BINARY_DIR}/engine-uninstall-files.nsh")
configure_file("${CMAKE_CURRENT_LIST_DIR}/engine-installer.nsi.in"
    "${CMAKE_BINARY_DIR}/engine-installer.nsi" @ONLY)

add_custom_target(engine-stage
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/stage-engine.cmake"
    DEPENDS ${_package_executables} ${_package_libraries}
    COMMENT "Assembling standalone mini-mbm-dev distribution" VERBATIM)

# Resolve NSIS only when packaging, so the portable targets need no installer tools.
file(WRITE "${CMAKE_BINARY_DIR}/make-engine-installer.cmake" "
find_program(MAKENSIS makensis PATHS \"C:/Program Files (x86)/NSIS\" \"C:/Program Files/NSIS\" REQUIRED)
execute_process(COMMAND \"\${MAKENSIS}\" \"${CMAKE_BINARY_DIR}/engine-installer.nsi\"
    COMMAND_ERROR_IS_FATAL ANY)
")
add_custom_target(engine-installer
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/make-engine-installer.cmake"
    DEPENDS engine-stage COMMENT "Building mini-mbm-dev NSIS installer" VERBATIM)
add_custom_target(engine-zip
    COMMAND ${CMAKE_COMMAND} -E tar cf "${ENGINE_PACKAGE_BASENAME}.zip" --format=zip
        mini-mbm-dev.EngineDir
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" DEPENDS engine-stage
    COMMENT "Building portable mini-mbm-dev ZIP" VERBATIM)
message(STATUS "Engine installer: cmake --build . --target engine-installer -> ${ENGINE_PACKAGE_BASENAME}-setup.exe")
