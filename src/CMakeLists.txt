cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(mini-mbm)

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

set(INCLUDE                     ${CMAKE_SOURCE_DIR}/include)
set(THIRD_PARTY                 ${CMAKE_SOURCE_DIR}/third-party)
set(PLUGINS                     ${CMAKE_SOURCE_DIR}/plugins)
set(CORE_MBM                    ${INCLUDE}/core_mbm)
set(RENDER_INCLUDES             ${INCLUDE}/render)
set(CORE_OPENGLES_INCLUDES      ${INCLUDE}/core_mbm)
set(LUA_INCLUDE                 ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(LUA_WRAP                    ${INCLUDE}/lua-interface)
set(TINYFILEDIALOGS_DIR         ${THIRD_PARTY}/tinyfiledialogs)


if (USE_LUA)

    set(LIB_MINIMBM_LUA_SRC         ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib.cpp)
    message(STATUS "Using LUA script...")
    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-linux/main-lua.cpp)
        set(LIB_MINIMBM_LUA_HEADER      ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib)
        set(LIB_MINIMBM_LUA_PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib-Linux.cpp)
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(MAIN                    ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main.cpp)
            set(GLES_INCLUDE            ${THIRD_PARTY}/gles)
        else()
            # MinGW / GCC: use a resource-free entry point
            set(MAIN                    ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main-mingw.cpp)
            set(LIB_MINIMBM_LUA_HEADER  ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib)
        endif()
        if(NOT USE_DIRECTX9)
            include(FindOpenGLES2)
        endif()
        set(LIB_MINIMBM_LUA_PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib-Windows.cpp)
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-android/main-native-activity.cpp)
    elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-macos/main-lua.cpp)
        set(LIB_MINIMBM_LUA_HEADER      ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib)
        set(LIB_MINIMBM_LUA_PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib-MacOs.mm)
        set_source_files_properties(${LIB_MINIMBM_LUA_PLATFORM_SRC}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-ios/main-lua.mm)
        set(LIB_MINIMBM_LUA_HEADER      ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib)
        set(LIB_MINIMBM_LUA_PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib-iOS.mm)
        set(IOS_PLATFORM_SOURCES
            ${CMAKE_SOURCE_DIR}/platform-ios/AppDelegate.mm
            ${CMAKE_SOURCE_DIR}/platform-ios/MBMMetalView.mm
            ${CMAKE_SOURCE_DIR}/platform-ios/MetalViewController.mm)
        # Apply ARC flag to ObjC++ files only — Swift files must not receive this flag.
        set_source_files_properties(${LIB_MINIMBM_LUA_PLATFORM_SRC} ${IOS_PLATFORM_SOURCES}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        # ── Swift native-command bridge (Xcode generator only) ────────────────
        # The make generator is used for CI / unsigned compile checks and does
        # not support Swift.  Skip everything below for that path.
        # ── ObjC native-command bridge (Xcode generator only) ────────────────
        # The make generator is used for CI / unsigned compile checks and does
        # not include the bridge.  The Xcode generator wires it up so that
        # mbm.doCommands() dispatches to MbmCommandBridge (built-in commands)
        # and then to MyCommands (game-specific stub in platform-ios/my-commands.m).
        if(CMAKE_GENERATOR STREQUAL "Xcode")
            add_definitions(-DMBM_IOS_SWIFT_BRIDGE=1)
            list(APPEND IOS_PLATFORM_SOURCES
                ${CMAKE_SOURCE_DIR}/platform-ios/MbmCommandBridge.mm
                ${CMAKE_SOURCE_DIR}/platform-ios/my-commands.m)
            # Both files are ObjC and must be compiled with ARC.
            set_source_files_properties(
                ${CMAKE_SOURCE_DIR}/platform-ios/MbmCommandBridge.mm
                ${CMAKE_SOURCE_DIR}/platform-ios/my-commands.m
                PROPERTIES COMPILE_FLAGS "-fobjc-arc")
            include_directories("${CMAKE_SOURCE_DIR}/platform-ios")
        endif()
    else()
        message(SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM ${ENGINE_TARGET_PLATFORM}")
    endif()

else()

    message(STATUS " Using pure CPP...")
    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-linux/main.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-linux/my-scene.cpp)
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(MAIN                ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main.cpp)
            set(GLES_INCLUDE        ${THIRD_PARTY}/gles)
        else()
            # MinGW / GCC: use a resource-free entry point
            set(MAIN                ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main-mingw.cpp)
            set(LIB_MINIMBM_LUA_HEADER ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib)
        endif()
        if(NOT USE_DIRECTX9)
            include(FindOpenGLES2)
        endif()
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-android/main-native-activity.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-android/my-scene.cpp)
    elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-macos/main.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-macos/my-scene.cpp)
    elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-ios/main-lua.mm)
        set(IOS_PLATFORM_SOURCES
            ${CMAKE_SOURCE_DIR}/platform-ios/AppDelegate.mm
            ${CMAKE_SOURCE_DIR}/platform-ios/MBMMetalView.mm
            ${CMAKE_SOURCE_DIR}/platform-ios/MetalViewController.mm
            ${CMAKE_SOURCE_DIR}/platform-ios/my-scene.mm)
        # iOS uses UIApplicationMain; my-scene.mm is compiled but active only when USE_LUA is off
        set_source_files_properties(${MAIN} ${IOS_PLATFORM_SOURCES}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    else()
        message(SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM ${ENGINE_TARGET_PLATFORM}")
    endif()

endif()

file(GLOB_RECURSE TINYFILEDIALOGS_SOURCES   "${TINYFILEDIALOGS_DIR}/*.c")

#Bring the headers, such as xxx.h into the project
include_directories(${INCLUDE}
                    #${GLES_INCLUDE}
                    ${RENDER_INCLUDES} 
                    ${CORE_OPENGLES_INCLUDES} 
                    ${THIRD_PARTY}
                    ${LUA_WRAP}
                    ${LUA_INCLUDE}
                    ${PLUGINS}
                    ${LIB_MINIMBM_LUA_HEADER})
if(OPENGLES2_INCLUDE_DIR AND NOT OPENGLES2_INCLUDE_DIR STREQUAL "OPENGLES2_INCLUDE_DIR-NOTFOUND")
    include_directories(${OPENGLES2_INCLUDE_DIR})
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
    include_directories(${INCLUDE}/plusWindows)
endif()

 
if (USE_LUA)
    file(GLOB_RECURSE SOURCES "lua-wrap/*.cpp")

    if(NOT USE_VR)
        get_filename_component(VR_LUA_CPP "lua-wrap/render-table/vr-lua.cpp" ABSOLUTE)
        list(REMOVE_ITEM SOURCES ${VR_LUA_CPP})
    endif()
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    #plugins must be added definitions here
    if(USE_LSQLITE3)
        add_definitions(-DUSE_LSQLITE3)
    endif()

    if(USE_BOX2D_LIQUID_FUN)
        add_definitions(-DUSE_BOX2D_LIQUID_FUN)
    endif()

    if(USE_BOX2D)
        add_definitions(-DUSE_BOX2D)
    endif()

    if(USE_BULLET3D)
        add_definitions(-DUSE_BULLET3D)
    endif()

    if(USE_IMGUI)
        add_definitions(-DUSE_IMGUI)
    endif()

    # android_native_app_glue: compile the glue code as a static lib so that
    # android_main() is called correctly by the NativeActivity launcher.
    set(NATIVE_APP_GLUE_SRC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    add_library(android_native_app_glue STATIC ${NATIVE_APP_GLUE_SRC})
    target_include_directories(android_native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue)

    add_library(mini-mbm  SHARED  ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${SCENE_CPP})

    target_include_directories(mini-mbm PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue)

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    # --whole-archive forces ANativeActivity_onCreate (defined in android_native_app_glue)
    # to be exported from the shared library. Without it the linker dead-strips the symbol
    # because nothing in our code directly calls it — Android calls it from outside.
    target_link_libraries(mini-mbm -Wl,--whole-archive android_native_app_glue -Wl,--no-whole-archive)
    target_link_libraries(mini-mbm android log EGL GLESv2)

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

    if (USE_LUA)
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${SCENE_CPP}
                            ${LIB_MINIMBM_LUA_SRC}
                            ${LIB_MINIMBM_LUA_PLATFORM_SRC})
    else()
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${SCENE_CPP})
    endif()
    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    target_link_libraries(mini-mbm Xrandr)

    # ── Linux game delivery (AppDir + optional AppImage) ──────────────────────
    # Activated only when -DGAME_ASSETS_DIR=/path/to/game/assets is passed at
    # configure time. Mirrors the iOS -DGAME_NAME / -DGAME_ASSETS_DIR / -DGAME_ICON_PNG
    # pattern so that each game can be packaged from outside the engine repo:
    #
    #   mkdir -p ~/tower-defense-linux && cd ~/tower-defense-linux
    #   cmake ~/mini-mbm \
    #       -DPLAT=Linux -DUSE_LUA=1 -DUSE_ALL=1 -DAUDIO=audiere \
    #       -DCMAKE_BUILD_TYPE=Release \
    #       -DGAME_NAME="Tower Defense Monster" \
    #       -DGAME_ASSETS_DIR=/home/michel/tower-defense/assets \
    #       -DGAME_ICON_PNG=/home/michel/tower-defense/icon.png
    #   make -j$(nproc)      # AppDir assembled automatically after build
    #   make appimage        # wrap into .AppImage (appimagetool required)
    #
    # Use an absolute path for GAME_ASSETS_DIR — CMake does not expand ~ inside
    # double-quoted -D values.  Use $HOME or the full path instead.

    if(DEFINED GAME_ASSETS_DIR)

        # ── Defaults ─────────────────────────────────────────────────────────
        if(NOT DEFINED GAME_NAME)
            set(GAME_NAME "mini-mbm")
        endif()

        # Expand leading ~ so users can write GAME_ASSETS_DIR=~/tower-defense/assets
        if(GAME_ASSETS_DIR MATCHES "^~/")
            string(REGEX REPLACE "^~/" "$ENV{HOME}/" GAME_ASSETS_DIR "${GAME_ASSETS_DIR}")
        endif()
        if(GAME_ASSETS_DIR AND NOT IS_ABSOLUTE "${GAME_ASSETS_DIR}")
            get_filename_component(GAME_ASSETS_DIR "${GAME_ASSETS_DIR}" ABSOLUTE)
        endif()
        if(NOT IS_DIRECTORY "${GAME_ASSETS_DIR}")
            message(WARNING "GAME_ASSETS_DIR '${GAME_ASSETS_DIR}' does not exist or is not a directory — no assets will be bundled. Check the path (tilde ~ is not expanded inside double-quoted cmake -D values).")
        endif()

        # Filesystem-safe name: spaces → underscores (used for dir/file names only)
        string(REPLACE " " "_" GAME_NAME_SAFE "${GAME_NAME}")

        set(_LINUX_APPDIR "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}.AppDir")

        # ── AppDir skeleton (configure time) ─────────────────────────────────
        file(MAKE_DIRECTORY "${_LINUX_APPDIR}/usr/bin")
        file(MAKE_DIRECTORY "${_LINUX_APPDIR}/usr/lib")
        file(MAKE_DIRECTORY "${_LINUX_APPDIR}/assets")

        # AppRun script — sets LD_LIBRARY_PATH, cds to AppDir, launches mini-mbm
        # with the bundled assets/main.lua and passes the game name as window title.
        file(WRITE "${_LINUX_APPDIR}/AppRun"
"#!/bin/bash
# Generated by mini-mbm cmake — do not edit manually.
APPDIR=\"$(dirname \"$(readlink -f \"\$0\")\")\"
export LD_LIBRARY_PATH=\"\$APPDIR/usr/lib:\$LD_LIBRARY_PATH\"

# Persistent writable directory for save files, preferences, etc.
# The AppImage filesystem is read-only; games must write here instead.
# Lua code can read this path with: os.getenv(\"GAME_SAVE_DIR\")
GAME_SAVE_DIR=\"\${XDG_DATA_HOME:-\$HOME/.local/share}/${GAME_NAME_SAFE}\"
mkdir -p \"\$GAME_SAVE_DIR\"
export GAME_SAVE_DIR

cd \"\$APPDIR\"
exec \"\$APPDIR/usr/bin/mini-mbm\" --name \"${GAME_NAME}\" --scene \"\$APPDIR/assets/main.lua\" \"\$@\"
")
        execute_process(COMMAND chmod +x "${_LINUX_APPDIR}/AppRun")

        # .desktop file — required by AppImage spec; also allows launching via
        # file managers that honour XDG desktop entries.
        file(WRITE "${_LINUX_APPDIR}/${GAME_NAME_SAFE}.desktop"
"[Desktop Entry]
Type=Application
Name=${GAME_NAME}
Exec=AppRun
Icon=${GAME_NAME_SAFE}
Categories=Game;
")

        # Optional icon — copied as-is (no alpha flattening needed on Linux)
        if(DEFINED GAME_ICON_PNG)
            if(GAME_ICON_PNG MATCHES "^~/")
                string(REGEX REPLACE "^~/" "$ENV{HOME}/" GAME_ICON_PNG "${GAME_ICON_PNG}")
            endif()
            if(NOT IS_ABSOLUTE "${GAME_ICON_PNG}")
                get_filename_component(GAME_ICON_PNG "${GAME_ICON_PNG}" ABSOLUTE)
            endif()
            if(EXISTS "${GAME_ICON_PNG}")
                file(COPY "${GAME_ICON_PNG}" DESTINATION "${_LINUX_APPDIR}")
                # AppImage tools look for a file whose stem matches the Icon= entry.
                # Rename the copy to <GAME_NAME_SAFE>.png so the stem matches.
                get_filename_component(_ICON_EXT "${GAME_ICON_PNG}" EXT)
                get_filename_component(_ICON_BASENAME "${GAME_ICON_PNG}" NAME)
                if(NOT "${_ICON_BASENAME}" STREQUAL "${GAME_NAME_SAFE}${_ICON_EXT}")
                    file(RENAME
                        "${_LINUX_APPDIR}/${_ICON_BASENAME}"
                        "${_LINUX_APPDIR}/${GAME_NAME_SAFE}${_ICON_EXT}")
                endif()
                message(STATUS "Linux icon        : ${GAME_ICON_PNG}")
            else()
                message(WARNING "GAME_ICON_PNG '${GAME_ICON_PNG}' does not exist — icon will not be bundled.")
            endif()
        endif()

        message(STATUS "Linux game name   : ${GAME_NAME}")
        message(STATUS "Linux assets dir  : ${GAME_ASSETS_DIR}")
        message(STATUS "Linux AppDir      : ${_LINUX_APPDIR}")
        message(STATUS "After 'make': run  ${_LINUX_APPDIR}/AppRun")
        message(STATUS "AppImage (opt.)   : cd ${CMAKE_BINARY_DIR} && make appimage")

        # ── POST_BUILD: populate AppDir after each successful build ───────────
        # 1. Copy the engine binary
        add_custom_command(TARGET mini-mbm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:mini-mbm>"
                "${_LINUX_APPDIR}/usr/bin/mini-mbm"
            COMMENT "Linux delivery: copying mini-mbm → AppDir/usr/bin/")

        # 2. Copy all shared-library plugins (.so) produced by this build,
        #    plus the audio runtime library if it needs to be bundled.
        #    Inline bash -c with \; and ; suffers from CMake→Makefile escaping issues.
        #    Instead, generate a small cmake -P script at configure time and invoke it.
        #
        #    Audiere (-DAUDIO=audiere):
        #      libaudiere-1.10.1.so is copied via the glob below.
        #      libasound is NOT bundled — Audiere finds it via dlopen() at runtime,
        #      and the system libasound2 (on all Debian/Ubuntu machines) must handle
        #      it so that PipeWire-ALSA routing works correctly.
        #
        #    PortAudio (-DAUDIO=portaudio):
        #      libportaudio.so.2 is a system library that may not be present on the
        #      target machine. It CAN be safely bundled (no runtime config/plugin
        #      path problem — unlike libasound). The system libasound.so.2 that
        #      portaudio depends on will be resolved from the host at runtime.
        if(AUDIO STREQUAL "portaudio")
            find_library(_LIBPORTAUDIO_PATH NAMES portaudio)
            if(_LIBPORTAUDIO_PATH)
                get_filename_component(_LIBPORTAUDIO_REAL "${_LIBPORTAUDIO_PATH}" REALPATH)
                message(STATUS "Linux delivery: bundling PortAudio (${_LIBPORTAUDIO_REAL})")
            else()
                set(_LIBPORTAUDIO_REAL "")
                message(WARNING "libportaudio not found — audio may not work in the AppImage. Install portaudio19-dev.")
            endif()
        else()
            set(_LIBPORTAUDIO_REAL "")
        endif()

        file(CONFIGURE
            OUTPUT "${CMAKE_BINARY_DIR}/copy_linux_plugins.cmake"
            CONTENT
"file(GLOB _SO_FILES \"@CMAKE_LIBRARY_OUTPUT_DIRECTORY@/*.so\")
foreach(_F \${_SO_FILES})
    # Skip audiere when portaudio is the selected audio backend.
    # A leftover libaudiere*.so in the build dir must not end up in the AppImage.
    if(_F MATCHES \"libaudiere\" AND \"@AUDIO@\" STREQUAL \"portaudio\")
        continue()
    endif()
    file(COPY \"\${_F}\" DESTINATION \"@_LINUX_APPDIR@/usr/lib/\")
endforeach()
# Remove any stale audiere library from the AppDir when using portaudio.
# (Guards against incremental builds where audiere was bundled by a prior run.)
if(\"@AUDIO@\" STREQUAL \"portaudio\")
    file(GLOB _STALE_AUDIERE \"@_LINUX_APPDIR@/usr/lib/libaudiere*.so*\")
    foreach(_F \${_STALE_AUDIERE})
        file(REMOVE \"\${_F}\")
    endforeach()
endif()
# Bundle PortAudio if this build uses -DAUDIO=portaudio
set(_LIBPORTAUDIO \"@_LIBPORTAUDIO_REAL@\")
if(_LIBPORTAUDIO AND EXISTS \"\${_LIBPORTAUDIO}\")
    file(COPY \"\${_LIBPORTAUDIO}\" DESTINATION \"@_LINUX_APPDIR@/usr/lib/\")
    get_filename_component(_PA_NAME \"\${_LIBPORTAUDIO}\" NAME)
    if(NOT _PA_NAME STREQUAL \"libportaudio.so.2\")
        file(CREATE_LINK \"\${_PA_NAME}\"
            \"@_LINUX_APPDIR@/usr/lib/libportaudio.so.2\" SYMBOLIC)
    endif()
endif()
"
            @ONLY)
        add_custom_command(TARGET mini-mbm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/copy_linux_plugins.cmake"
            COMMENT "Linux delivery: copying plugins (.so) → AppDir/usr/lib/")

        # 3. Copy game assets
        add_custom_command(TARGET mini-mbm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GAME_ASSETS_DIR}"
                "${_LINUX_APPDIR}/assets"
            COMMENT "Linux delivery: copying assets from ${GAME_ASSETS_DIR} → AppDir/assets/")

        # ── 'appimage' convenience target ─────────────────────────────────────
        # Wraps the AppDir into a self-contained .AppImage using appimagetool.
        # appimagetool is searched in:
        #   1. $PATH (system-wide install)
        #   2. ${CMAKE_BINARY_DIR}/appimagetool-x86_64.AppImage  (local download)
        # If neither is found the target prints a download URL and exits cleanly.
        # Download: https://github.com/AppImage/AppImageKit/releases/latest
        #
        # The logic is written to a generated shell script at configure time so that
        # semicolons and backslashes are not re-escaped by CMake's Makefile generator.
        file(WRITE "${CMAKE_BINARY_DIR}/make_appimage.sh"
"#!/bin/bash
# Generated by mini-mbm cmake — do not edit manually.
TOOL=
if command -v appimagetool >/dev/null 2>&1; then
    TOOL=appimagetool
elif [ -f '${CMAKE_BINARY_DIR}/appimagetool-x86_64.AppImage' ]; then
    chmod +x '${CMAKE_BINARY_DIR}/appimagetool-x86_64.AppImage'
    TOOL='${CMAKE_BINARY_DIR}/appimagetool-x86_64.AppImage --appimage-extract-and-run'
fi
if [ -n \"\$TOOL\" ]; then
    ARCH=x86_64 \$TOOL '${_LINUX_APPDIR}' '${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}-x86_64.AppImage' &&
    echo '--- AppImage ready: ${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}-x86_64.AppImage'
else
    echo 'appimagetool not found.'
    echo 'Download: https://github.com/AppImage/AppImageKit/releases/latest'
    echo 'Place appimagetool-x86_64.AppImage in ${CMAKE_BINARY_DIR}/ and re-run: make appimage'
    echo 'Standalone AppDir already ready at: ${_LINUX_APPDIR}'
fi
")
        execute_process(COMMAND chmod +x "${CMAKE_BINARY_DIR}/make_appimage.sh")
        add_custom_target(appimage
            DEPENDS mini-mbm
            COMMAND bash "${CMAKE_BINARY_DIR}/make_appimage.sh"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Building AppImage: ${GAME_NAME_SAFE}-x86_64.AppImage")

    endif() # GAME_ASSETS_DIR

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")

    # Enable Windows resource compiler (windres on MinGW, rc.exe on MSVC) so
    # that .rc files added later (for icon embedding) are compiled correctly.
    enable_language(RC)

    # tinyfiledialogs is already compiled into core_mbm.dll — don't add it again.
    if (USE_LUA)
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${SCENE_CPP}
                            ${LIB_MINIMBM_LUA_SRC}
                            ${LIB_MINIMBM_LUA_PLATFORM_SRC})
    else()
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${SCENE_CPP})
    endif()

    # mini-mbm-lib sources are compiled directly into this exe (not a DLL),
    # so define LIB_MBM_EXPORTS to get dllexport (not dllimport) on GCC/MinGW.
    target_compile_definitions(mini-mbm PRIVATE LIB_MBM_EXPORTS)

    # Generate a small .cpp that exposes the baked-in game name as a C symbol.
    # Using file(WRITE ...) avoids all shell/CMake quoting pitfalls that come
    # with -D defines for strings containing spaces.
    # Falls back to "Mini MBM" when GAME_NAME is not set (raw engine build).
    if(DEFINED GAME_NAME AND NOT "${GAME_NAME}" STREQUAL "")
        set(_MBM_BAKED_NAME "${GAME_NAME}")
    else()
        set(_MBM_BAKED_NAME "Mini MBM")
    endif()
    file(WRITE "${CMAKE_BINARY_DIR}/mbm_game_name.cpp"
        "extern const char mbm_baked_game_name[] = \"${_MBM_BAKED_NAME}\";\n")
    target_sources(mini-mbm PRIVATE "${CMAKE_BINARY_DIR}/mbm_game_name.cpp")

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})

    # Build as a WINDOWS-subsystem PE so the OS never allocates a console.
    # -Wl,--subsystem,windows sets the subsystem without changing the entry
    # point (mainCRTStartup still calls main()), unlike -mwindows which
    # would require WinMain.
    target_link_options(mini-mbm PRIVATE -Wl,--subsystem,windows)

    # ── Windows game delivery (GameDir + NSIS installer + WiX MSI + ZIP) ──────
    # Activated only when -DGAME_ASSETS_DIR=/path/to/game/assets is passed at
    # configure time.  Mirrors the Linux/iOS -DGAME_NAME / -DGAME_ASSETS_DIR /
    # -DGAME_ICON_PNG pattern so that each game can be packaged from outside the
    # engine repo:
    #
    #   mkdir build\my_game_win && cd build\my_game_win
    #   cmake ..\.. -G "MinGW Makefiles"
    #       -DPLAT=Windows -DUSE_ALL=1 -DAUDIO=audiere
    #       -DCMAKE_BUILD_TYPE=Release
    #       -DGAME_NAME="Tower Defense Monster"
    #       -DGAME_ASSETS_DIR=C:/path/to/tower-defense/assets
    #       -DGAME_ICON_PNG=C:/path/to/tower-defense/icon.png
    #   mingw32-make -j%NUMBER_OF_PROCESSORS%   # GameDir assembled after build
    #   mingw32-make nsis                        # produces GameName-windows-setup.exe
    #   mingw32-make msi                         # produces GameName-windows.msi
    #   mingw32-make zip                         # produces GameName-windows.zip
    #
    # Supply -DGAME_ICON_ICO=C:/path/icon.ico to skip ImageMagick conversion.
    # If only -DGAME_ICON_PNG is given, ImageMagick convert is tried automatically.

    if(DEFINED GAME_ASSETS_DIR)

        # ── Defaults ─────────────────────────────────────────────────────────
        if(NOT DEFINED GAME_NAME)
            set(GAME_NAME "mini-mbm")
        endif()

        if(GAME_ASSETS_DIR AND NOT IS_ABSOLUTE "${GAME_ASSETS_DIR}")
            get_filename_component(GAME_ASSETS_DIR "${GAME_ASSETS_DIR}" ABSOLUTE)
        endif()
        if(NOT IS_DIRECTORY "${GAME_ASSETS_DIR}")
            message(WARNING "GAME_ASSETS_DIR '${GAME_ASSETS_DIR}' does not exist or is not a directory — no assets will be bundled.")
        endif()

        # Filesystem-safe name: spaces → underscores
        string(REPLACE " " "_" GAME_NAME_SAFE "${GAME_NAME}")

        set(_WIN_GAMEDIR "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}.GameDir")

        # ── Icon resolution ───────────────────────────────────────────────────
        # Priority: explicit .ico > ImageMagick-converted .png > no icon
        set(_WIN_ICON_ICO "")
        if(DEFINED GAME_ICON_ICO)
            if(NOT IS_ABSOLUTE "${GAME_ICON_ICO}")
                get_filename_component(GAME_ICON_ICO "${GAME_ICON_ICO}" ABSOLUTE)
            endif()
            if(EXISTS "${GAME_ICON_ICO}")
                set(_WIN_ICON_ICO "${GAME_ICON_ICO}")
                message(STATUS "Windows icon (ICO)  : ${_WIN_ICON_ICO}")
            else()
                message(WARNING "GAME_ICON_ICO '${GAME_ICON_ICO}' not found — icon will not be bundled.")
            endif()
        elseif(DEFINED GAME_ICON_PNG)
            if(NOT IS_ABSOLUTE "${GAME_ICON_PNG}")
                get_filename_component(GAME_ICON_PNG "${GAME_ICON_PNG}" ABSOLUTE)
            endif()
            if(EXISTS "${GAME_ICON_PNG}")
                # Prefer 'magick' (ImageMagick 7+) which is unique to ImageMagick —
                # 'convert' collides with the Windows built-in System32\convert.exe
                # (the disk-partition utility) and must be avoided with NO_DEFAULT_PATH.
                find_program(IMAGEMAGICK_CONVERT
                    NAMES magick
                )
                if(NOT IMAGEMAGICK_CONVERT)
                    find_program(IMAGEMAGICK_CONVERT
                        NAMES convert
                        HINTS
                            "C:/Program Files/ImageMagick-7.1.0-Q16-HDRI"
                            "C:/Program Files/ImageMagick-7.0.11-Q16-HDRI"
                            "C:/Program Files (x86)/ImageMagick-7.1.0-Q16-HDRI"
                            "C:/Program Files/ImageMagick-7.1.1-Q16-HDRI"
                        NO_DEFAULT_PATH
                    )
                endif()
                if(IMAGEMAGICK_CONVERT)
                    set(_WIN_ICON_ICO "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}.ico")
                    execute_process(
                        COMMAND "${IMAGEMAGICK_CONVERT}" "${GAME_ICON_PNG}"
                            -define icon:auto-resize=256,128,96,64,48,32,16
                            "${_WIN_ICON_ICO}"
                        RESULT_VARIABLE _ICO_RESULT
                    )
                    if(NOT _ICO_RESULT EQUAL 0)
                        message(WARNING "ImageMagick failed to convert icon — no .ico will be bundled.")
                        set(_WIN_ICON_ICO "")
                    else()
                        message(STATUS "Windows icon (PNG→ICO): ${_WIN_ICON_ICO}")
                    endif()
                else()
                    message(STATUS "ImageMagick not found — supply -DGAME_ICON_ICO=... or install ImageMagick for a Windows icon.")
                endif()
            else()
                message(WARNING "GAME_ICON_PNG '${GAME_ICON_PNG}' not found.")
            endif()
        endif()

        # ── Embed icon into the executable ───────────────────────────────────
        # A Windows .rc resource file is generated at configure time and linked
        # into the exe so that Explorer shows the correct icon on the .exe file.
        if(_WIN_ICON_ICO AND EXISTS "${_WIN_ICON_ICO}")
            set(_RC_FILE "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}_icon.rc")
            # Use numeric ID 101 so that LoadIcon/MAKEINTRESOURCE(101) works at
            # runtime when main-mingw.cpp calls mbm::set_icon(IDI_ICON1) with
            # IDI_ICON1 defined as 101.  String identifiers (IDI_ICON1 without
            # a numeric definition) are not found by MAKEINTRESOURCE.
            string(REPLACE "\\" "/" _RC_ICON_PATH_FWD "${_WIN_ICON_ICO}")
            file(WRITE "${_RC_FILE}" "101 ICON \"${_RC_ICON_PATH_FWD}\"\n")
            target_sources(mini-mbm PRIVATE "${_RC_FILE}")
            message(STATUS "Windows icon (embed): ${_WIN_ICON_ICO}")
        endif()

        # ── Staging directory skeleton ────────────────────────────────────────
        file(MAKE_DIRECTORY "${_WIN_GAMEDIR}/assets")

        # launch.bat — sets save dir, cds to GameDir, launches the renamed exe.
        # Lua code can read: os.getenv("GAME_SAVE_DIR")
        file(WRITE "${_WIN_GAMEDIR}/launch.bat"
"@echo off\r\n\
rem Generated by mini-mbm cmake -- do not edit manually.\r\n\
set \"GAME_SAVE_DIR=%APPDATA%\\${GAME_NAME_SAFE}\"\r\n\
if not exist \"%GAME_SAVE_DIR%\" mkdir \"%GAME_SAVE_DIR%\"\r\n\
cd /d \"%~dp0\"\r\n\
\"${GAME_NAME_SAFE}.exe\" --name \"${GAME_NAME}\" --scene \"assets\\main.lua\" %*\r\n\
")

        message(STATUS "Windows game name   : ${GAME_NAME}")
        message(STATUS "Windows assets dir  : ${GAME_ASSETS_DIR}")
        message(STATUS "Windows GameDir     : ${_WIN_GAMEDIR}")

        # ── DLL source paths ─────────────────────────────────────────────────
        set(_WIN_DLL_LIST "")
        if(AUDIO STREQUAL "audiere")
            # audiere ships only a 32-bit DLL in the repo — only add it for x86 builds.
            if(CMAKE_SIZEOF_VOID_P EQUAL 4)
                list(APPEND _WIN_DLL_LIST "${THIRD_PARTY}/audiere-1.9.4/bin/audiere.dll")
            else()
                message(WARNING "audiere has no bundled 64-bit DLL. Use -DAUDIO=dsound for 64-bit Windows builds.")
            endif()
        endif()
        # dsound is a system DLL — no file to copy.
        if(USE_DIRECTX9)
            # Always use the system D3DCompiler_47.dll — it matches the process bitness.
            list(APPEND _WIN_DLL_LIST "$ENV{SystemRoot}/System32/D3DCompiler_47.dll")
        else()
            # OpenGlEs is wrapper over D3DCompiler_47.dll — so, we also copy it.
            list(APPEND _WIN_DLL_LIST "$ENV{SystemRoot}/System32/D3DCompiler_47.dll")
            list(APPEND _WIN_DLL_LIST
                "${THIRD_PARTY}/gles/bin/libEGL.dll"
                "${THIRD_PARTY}/gles/bin/libGLESv2.dll")
        endif()

        # MinGW runtime DLLs — needed when the exe was compiled with GCC/MinGW.
        # Without these, the game crashes with "DLL not found" when launched from
        # Explorer on a machine that does not have MSYS2/MinGW on its PATH.
        #
        # libgcc_s variant depends on the exception model / architecture:
        #   libgcc_s_dw2-1.dll  — 32-bit (dw2 / sjlj), mingw32
        #   libgcc_s_seh-1.dll  — 64-bit (SEH), mingw64 / ucrt64
        #   libgcc_s_deh-1.dll  — 64-bit (SJLJ, uncommon)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            get_filename_component(_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
            # Resolve the correct libgcc_s variant — use whichever one exists.
            set(_LIBGCC_DLL "")
            foreach(_CANDIDATE libgcc_s_dw2-1.dll libgcc_s_seh-1.dll libgcc_s_deh-1.dll)
                if(EXISTS "${_MINGW_BIN}/${_CANDIDATE}")
                    set(_LIBGCC_DLL "${_CANDIDATE}")
                    break()
                endif()
            endforeach()
            if(_LIBGCC_DLL)
                list(APPEND _WIN_DLL_LIST "${_MINGW_BIN}/${_LIBGCC_DLL}")
            else()
                message(WARNING "MinGW runtime DLL not found (skipped): no libgcc_s variant in ${_MINGW_BIN}")
            endif()
            foreach(_RT_DLL libstdc++-6.dll libwinpthread-1.dll)
                if(EXISTS "${_MINGW_BIN}/${_RT_DLL}")
                    list(APPEND _WIN_DLL_LIST "${_MINGW_BIN}/${_RT_DLL}")
                else()
                    message(WARNING "MinGW runtime DLL not found (skipped): ${_MINGW_BIN}/${_RT_DLL}")
                endif()
            endforeach()
        endif()

        # ── POST_BUILD: populate GameDir after each successful build ──────────
        # 1. Copy and rename mini-mbm.exe → GameName_Safe.exe
        add_custom_command(TARGET mini-mbm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:mini-mbm>"
                "${_WIN_GAMEDIR}/${GAME_NAME_SAFE}.exe"
            COMMENT "Windows delivery: mini-mbm.exe → GameDir/${GAME_NAME_SAFE}.exe")

        # 2. Copy runtime DLLs
        foreach(_DLL ${_WIN_DLL_LIST})
            if(EXISTS "${_DLL}")
                add_custom_command(TARGET mini-mbm POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy "${_DLL}" "${_WIN_GAMEDIR}/"
                    COMMENT "Windows delivery: copying ${_DLL}")
            else()
                message(WARNING "Runtime DLL not found (will be skipped): ${_DLL}")
            endif()
        endforeach()

        # 3. Copy plugin DLLs from the build output directory.
        #    Generated cmake -P script runs after build so the DLLs exist.
        file(CONFIGURE
            OUTPUT "${CMAKE_BINARY_DIR}/copy_win_plugins.cmake"
            CONTENT
"file(GLOB _DLL_FILES \"@CMAKE_LIBRARY_OUTPUT_DIRECTORY@/*.dll\")
foreach(_F \${_DLL_FILES})
    file(COPY \"\${_F}\" DESTINATION \"@_WIN_GAMEDIR@/\")
endforeach()
"
            @ONLY)
        add_custom_command(TARGET mini-mbm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/copy_win_plugins.cmake"
            COMMENT "Windows delivery: copying plugins (.dll) → GameDir/")

        # 4. Copy game assets
        add_custom_command(TARGET mini-mbm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GAME_ASSETS_DIR}"
                "${_WIN_GAMEDIR}/assets"
            COMMENT "Windows delivery: copying assets from ${GAME_ASSETS_DIR} → GameDir/assets/")

        # 5. Copy icon
        if(_WIN_ICON_ICO AND EXISTS "${_WIN_ICON_ICO}")
            add_custom_command(TARGET mini-mbm POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${_WIN_ICON_ICO}"
                    "${_WIN_GAMEDIR}/${GAME_NAME_SAFE}.ico"
                COMMENT "Windows delivery: copying icon → GameDir/${GAME_NAME_SAFE}.ico")
        endif()

        # ── NSIS installer ────────────────────────────────────────────────────
        set(_NSIS_ICON_LINE "")
        if(_WIN_ICON_ICO AND EXISTS "${_WIN_ICON_ICO}")
            string(REPLACE "/" "\\" _NSIS_ICON_PATH "${_WIN_GAMEDIR}/${GAME_NAME_SAFE}.ico")
            set(_NSIS_ICON_LINE "!define MUI_ICON \"${_NSIS_ICON_PATH}\"")
        endif()
        string(REPLACE "/" "\\" _NSIS_GAMEDIR_WIN "${_WIN_GAMEDIR}")
        string(REPLACE "/" "\\" _NSIS_OUTPUT_WIN  "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}-windows-setup.exe")

        file(WRITE "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}.nsi"
"!include \"MUI2.nsh\"\r\n\
\r\n\
; Generated by mini-mbm cmake -- do not edit manually.\r\n\
Name \"${GAME_NAME}\"\r\n\
OutFile \"${_NSIS_OUTPUT_WIN}\"\r\n\
Unicode True\r\n\
; Per-user install — no UAC prompt, no admin rights required.\r\n\
; Installs to %LOCALAPPDATA%\\Programs\\<GameName>.\r\n\
InstallDir \"\$LOCALAPPDATA\\Programs\\${GAME_NAME_SAFE}\"\r\n\
InstallDirRegKey HKCU \"Software\\${GAME_NAME_SAFE}\" \"\"\r\n\
RequestExecutionLevel user\r\n\
\r\n\
${_NSIS_ICON_LINE}\r\n\
!define MUI_ABORTWARNING\r\n\
!insertmacro MUI_PAGE_DIRECTORY\r\n\
!insertmacro MUI_PAGE_INSTFILES\r\n\
!insertmacro MUI_UNPAGE_CONFIRM\r\n\
!insertmacro MUI_UNPAGE_INSTFILES\r\n\
!insertmacro MUI_LANGUAGE \"English\"\r\n\
\r\n\
Section \"Install\"\r\n\
    SetOutPath \"\$INSTDIR\"\r\n\
    File /r \"${_NSIS_GAMEDIR_WIN}\\*.*\"\r\n\
\r\n\
    WriteUninstaller \"\$INSTDIR\\uninstall.exe\"\r\n\
\r\n\
    ; Start-menu and desktop shortcuts (per-user shell folders — no admin needed)\r\n\
    SetShellVarContext current\r\n\
    CreateDirectory \"\$SMPROGRAMS\\${GAME_NAME}\"\r\n\
    CreateShortcut \"\$SMPROGRAMS\\${GAME_NAME}\\${GAME_NAME}.lnk\" \"\$INSTDIR\\${GAME_NAME_SAFE}.exe\" \"--name $\\\"${GAME_NAME}$\\\" --scene assets\\main.lua\" \"\$INSTDIR\\${GAME_NAME_SAFE}.ico\"\r\n\
    CreateShortcut \"\$DESKTOP\\${GAME_NAME}.lnk\" \"\$INSTDIR\\${GAME_NAME_SAFE}.exe\" \"--name $\\\"${GAME_NAME}$\\\" --scene assets\\main.lua\" \"\$INSTDIR\\${GAME_NAME_SAFE}.ico\"\r\n\
\r\n\
    ; Uninstall entry in HKCU (no admin rights needed)\r\n\
    WriteRegStr HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${GAME_NAME_SAFE}\" \"DisplayName\" \"${GAME_NAME}\"\r\n\
    WriteRegStr HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${GAME_NAME_SAFE}\" \"UninstallString\" \"\$INSTDIR\\uninstall.exe\"\r\n\
SectionEnd\r\n\
\r\n\
Section \"Uninstall\"\r\n\
    SetShellVarContext current\r\n\
    Delete \"\$INSTDIR\\uninstall.exe\"\r\n\
    RMDir /r \"\$INSTDIR\"\r\n\
    Delete \"\$SMPROGRAMS\\${GAME_NAME}\\${GAME_NAME}.lnk\"\r\n\
    RMDir \"\$SMPROGRAMS\\${GAME_NAME}\"\r\n\
    Delete \"\$DESKTOP\\${GAME_NAME}.lnk\"\r\n\
    DeleteRegKey HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${GAME_NAME_SAFE}\"\r\n\
    DeleteRegKey HKCU \"Software\\${GAME_NAME_SAFE}\"\r\n\
SectionEnd\r\n\
")

        # make_nsis.cmake — executed at `make nsis` time
        file(CONFIGURE
            OUTPUT "${CMAKE_BINARY_DIR}/make_nsis.cmake"
            CONTENT
"cmake_minimum_required(VERSION 3.25.1)
set(_NSI_FILE \"@CMAKE_BINARY_DIR@/@GAME_NAME_SAFE@.nsi\")
set(_OUT_FILE \"@CMAKE_BINARY_DIR@/@GAME_NAME_SAFE@-windows-setup.exe\")

# Locate makensis: PATH first, then common install locations
find_program(_MAKENSIS makensis
    PATHS
        \"C:/Program Files (x86)/NSIS\"
        \"C:/Program Files/NSIS\"
)
if(_MAKENSIS)
    execute_process(
        COMMAND \"\${_MAKENSIS}\" \"\${_NSI_FILE}\"
        RESULT_VARIABLE _RES)
    if(_RES EQUAL 0)
        message(STATUS \"--- NSIS installer ready: \${_OUT_FILE}\")
    else()
        message(FATAL_ERROR \"makensis failed with code \${_RES}\")
    endif()
else()
    message(STATUS \"makensis not found.\")
    message(STATUS \"Download NSIS: https://nsis.sourceforge.io\")
    message(STATUS \"Standalone GameDir already ready at: @_WIN_GAMEDIR@\")
endif()
"
            @ONLY)

        add_custom_target(nsis
            DEPENDS mini-mbm
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/make_nsis.cmake"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Building NSIS installer: ${GAME_NAME_SAFE}-windows-setup.exe")

        # ── WiX MSI ──────────────────────────────────────────────────────────
        # The .wxs is generated at `make msi` time (after POST_BUILD) by a
        # cmake -P script that enumerates the fully-populated GameDir.
        # A fixed namespace UUID for deterministic per-file component GUIDs.
        set(_MBM_WIX_NS "6BA7B810-9DAD-11D1-80B4-00C04FD430C8")
        string(UUID _WIX_UPGRADE_CODE
            NAMESPACE "${_MBM_WIX_NS}"
            NAME      "mini-mbm-upgrade-${GAME_NAME_SAFE}"
            TYPE SHA1 UPPER)

        # Pass just the ICO path into the generated script; the script builds
        # the XML itself so that no unescaped quotes end up inside a CMake
        # set() call (which was causing "Syntax Warning" at make msi time).
        set(_WIX_ICON_ICO_PATH "")
        if(_WIN_ICON_ICO AND EXISTS "${_WIN_ICON_ICO}")
            set(_WIX_ICON_ICO_PATH "${_WIN_GAMEDIR}/${GAME_NAME_SAFE}.ico")
        endif()

        # Write the cmake -P script; @VAR@ → substituted now, ${VAR} → runtime
        file(CONFIGURE
            OUTPUT "${CMAKE_BINARY_DIR}/make_wix.cmake"
            CONTENT
[==[cmake_minimum_required(VERSION 3.25.1)
# ── Values injected at configure time ────────────────────────────────────────
set(GAME_NAME          "@GAME_NAME@")
set(GAME_NAME_SAFE     "@GAME_NAME_SAFE@")
set(WIN_GAMEDIR        "@_WIN_GAMEDIR@")
set(WIX_OUTPUT_MSI     "@CMAKE_BINARY_DIR@/@GAME_NAME_SAFE@-windows.msi")
set(WIX_OUTPUT_WXS     "@CMAKE_BINARY_DIR@/@GAME_NAME_SAFE@.wxs")
set(WIX_UPGRADE_CODE   "@_WIX_UPGRADE_CODE@")
set(MBM_WIX_NS         "@_MBM_WIX_NS@")
set(WIX_ICON_ICO_PATH  "@_WIX_ICON_ICO_PATH@")

# ── Detect WiX ───────────────────────────────────────────────────────────────
# Use NO_CACHE so find_program writes to a regular variable; without it, the
# set("") above hides the cache result and the variable stays empty.
find_program(WIX_V4 wix NO_CACHE)
if(NOT WIX_V4)
    # Collect bin/ dirs from every WiX v3.x installation found under Program Files.
    file(GLOB _WIX3_DIRS
        "C:/Program Files (x86)/WiX Toolset v3*/bin"
        "C:/Program Files/WiX Toolset v3*/bin")
    find_program(WIX_CANDLE candle PATHS ${_WIX3_DIRS} NO_CACHE)
    find_program(WIX_LIGHT  light  PATHS ${_WIX3_DIRS} NO_CACHE)
endif()
if(NOT WIX_V4 AND NOT WIX_CANDLE)
    message(STATUS "WiX not found.")
    message(STATUS "  WiX v4 (recommended): dotnet tool install --global wix")
    message(STATUS "                        https://wixtoolset.org")
    message(STATUS "  WiX v3: https://github.com/wixtoolset/wix3/releases")
    message(STATUS "Standalone GameDir ready at: ${WIN_GAMEDIR}")
    return()
endif()

# ── Helpers ───────────────────────────────────────────────────────────────────
function(path_to_id OUT_VAR prefix rel_path)
    # WiX v3 identifiers: only [A-Za-z0-9_.] allowed — replace everything else.
    string(REGEX REPLACE "[^A-Za-z0-9_.]" "_" _id "${rel_path}")
    set(${OUT_VAR} "${prefix}_${_id}" PARENT_SCOPE)
endfunction()

function(make_guid OUT_VAR name)
    string(UUID _g NAMESPACE "${MBM_WIX_NS}"
           NAME "comp-${GAME_NAME_SAFE}-${name}" TYPE SHA1 UPPER)
    set(${OUT_VAR} "{${_g}}" PARENT_SCOPE)
endfunction()

# ── Enumerate all files in the populated GameDir ──────────────────────────────
file(GLOB_RECURSE _ALL_FILES LIST_DIRECTORIES false "${WIN_GAMEDIR}/*")
list(SORT _ALL_FILES)

# ── Build the nested <Directory> + <Component> XML ────────────────────────────
set(_CURRENT_DIR "")
set(WXS_BODY "")
set(WXS_COMP_IDS "")

foreach(_F ${_ALL_FILES})
    file(RELATIVE_PATH _REL "${WIN_GAMEDIR}" "${_F}")
    get_filename_component(_DIR  "${_REL}" DIRECTORY)
    # Replace backslash with forward slash for consistent handling
    string(REPLACE "\\" "/" _DIR  "${_DIR}")
    string(REPLACE "\\" "/" _REL  "${_REL}")

    # Transition directories: only close/open at the diverging level so that
    # parent directories shared with the previous path are not re-declared.
    if(NOT "${_DIR}" STREQUAL "${_CURRENT_DIR}")
        # Split both paths into parts
        if(_CURRENT_DIR)
            string(REPLACE "/" ";" _PREV_PARTS "${_CURRENT_DIR}")
        else()
            set(_PREV_PARTS "")
        endif()
        if(_DIR)
            string(REPLACE "/" ";" _NEXT_PARTS "${_DIR}")
        else()
            set(_NEXT_PARTS "")
        endif()
        list(LENGTH _PREV_PARTS _PREV_LEN)
        list(LENGTH _NEXT_PARTS _NEXT_LEN)
        # Count common leading parts
        set(_COMMON 0)
        set(_MORE TRUE)
        while(_MORE)
            if(_COMMON GREATER_EQUAL _PREV_LEN OR _COMMON GREATER_EQUAL _NEXT_LEN)
                set(_MORE FALSE)
            else()
                list(GET _PREV_PARTS ${_COMMON} _PP)
                list(GET _NEXT_PARTS ${_COMMON} _NP)
                if("${_PP}" STREQUAL "${_NP}")
                    math(EXPR _COMMON "${_COMMON} + 1")
                else()
                    set(_MORE FALSE)
                endif()
            endif()
        endwhile()
        # Close dirs from _CURRENT_DIR down to the common prefix
        math(EXPR _CLOSE "${_PREV_LEN} - ${_COMMON}")
        if(_CLOSE GREATER 0)
            foreach(_I RANGE 1 ${_CLOSE})
                string(APPEND WXS_BODY "            </Directory>\n")
            endforeach()
        endif()
        # Open dirs from the common prefix up to _DIR
        math(EXPR _OPEN_END "${_NEXT_LEN} - 1")
        if(_NEXT_LEN GREATER _COMMON)
            # Rebuild acc from the beginning up to (but not including) the common prefix
            set(_ACC "")
            if(_COMMON GREATER 0)
                math(EXPR _LAST_COMMON "${_COMMON} - 1")
                foreach(_J RANGE 0 ${_LAST_COMMON})
                    list(GET _NEXT_PARTS ${_J} _PART)
                    if(_ACC)
                        set(_ACC "${_ACC}/${_PART}")
                    else()
                        set(_ACC "${_PART}")
                    endif()
                endforeach()
            endif()
            # Open the new sub-directories starting at _COMMON
            foreach(_J RANGE ${_COMMON} ${_OPEN_END})
                list(GET _NEXT_PARTS ${_J} _PART)
                if(_ACC)
                    set(_ACC "${_ACC}/${_PART}")
                else()
                    set(_ACC "${_PART}")
                endif()
                path_to_id(_DID "Dir" "${_ACC}")
                string(APPEND WXS_BODY "            <Directory Id=\"${_DID}\" Name=\"${_PART}\">\n")
            endforeach()
        endif()
        set(_CURRENT_DIR "${_DIR}")
    endif()

    path_to_id(_CID "Comp" "${_REL}")
    path_to_id(_FID "File" "${_REL}")
    make_guid(_GUID "${_REL}")
    string(APPEND WXS_BODY
        "                <Component Id=\"${_CID}\" Guid=\"${_GUID}\">\n"
        "                    <File Id=\"${_FID}\" Source=\"${_F}\" KeyPath=\"yes\" />\n"
        "                </Component>\n")
    list(APPEND WXS_COMP_IDS "${_CID}")
endforeach()

# Close remaining open directories
if(_CURRENT_DIR)
    string(REPLACE "/" ";" _CLOSE_PARTS "${_CURRENT_DIR}")
    list(LENGTH _CLOSE_PARTS _DEPTH)
    foreach(_I RANGE 1 ${_DEPTH})
        string(APPEND WXS_BODY "            </Directory>\n")
    endforeach()
endif()

# ── Shortcut component (separate from file components — WiX best practice) ───
make_guid(_SC_GUID "shortcuts-${GAME_NAME_SAFE}")
set(_SHORTCUT_COMP
"        <DirectoryRef Id=\"ProgramMenuDir\">
            <Component Id=\"Comp_Shortcuts\" Guid=\"${_SC_GUID}\">
                <Shortcut Id=\"StartMenuShortcut\"
                          Name=\"${GAME_NAME}\"
                          Description=\"Play ${GAME_NAME}\"
                          Target=\"[INSTALLDIR]${GAME_NAME_SAFE}.exe\"
                          Arguments=\"--name &quot;${GAME_NAME}&quot; --scene assets\\main.lua\"
                          WorkingDirectory=\"INSTALLDIR\" />
                <Shortcut Id=\"DesktopShortcut\"
                          Name=\"${GAME_NAME}\"
                          Description=\"Play ${GAME_NAME}\"
                          Target=\"[INSTALLDIR]${GAME_NAME_SAFE}.exe\"
                          Arguments=\"--name &quot;${GAME_NAME}&quot; --scene assets\\main.lua\"
                          WorkingDirectory=\"INSTALLDIR\"
                          Directory=\"DesktopFolder\" />
                <Shortcut Id=\"UninstallShortcut\"
                          Name=\"Uninstall ${GAME_NAME}\"
                          Description=\"Uninstall ${GAME_NAME}\"
                          Target=\"[SystemFolder]msiexec.exe\"
                          Arguments=\"/x [ProductCode]\" />
                <RemoveFolder Id=\"RemoveProgramMenuDir\" Directory=\"ProgramMenuDir\" On=\"uninstall\" />
                <RegistryValue Root=\"HKCU\" Key=\"Software\\${GAME_NAME_SAFE}\"
                               Name=\"installed\" Type=\"integer\" Value=\"1\" KeyPath=\"yes\" />
            </Component>
        </DirectoryRef>
")

# ── Component refs ────────────────────────────────────────────────────────────
set(_COMP_REFS "")
foreach(_CID ${WXS_COMP_IDS})
    string(APPEND _COMP_REFS "            <ComponentRef Id=\"${_CID}\" />\n")
endforeach()
string(APPEND _COMP_REFS "            <ComponentRef Id=\"Comp_Shortcuts\" />\n")

# ── Build icon XML (no quotes inside a CMake set() — safe to interpolate) ───
set(_WIX_ICON_XML "")
if(NOT "${WIX_ICON_ICO_PATH}" STREQUAL "")
    set(_WIX_ICON_XML
        "        <Icon Id=\"AppIcon.ico\" SourceFile=\"${WIX_ICON_ICO_PATH}\" />\n        <Property Id=\"ARPPRODUCTICON\" Value=\"AppIcon.ico\" />")
endif()

# ── Generate license RTF shown in the WiX setup dialog ───────────────────────
# WixUI_Minimal always shows a license page; without this the installer shows
# Lorem ipsum placeholder text that ships with the WiX UI extension.
# We provide the MIT license as a minimal RTF so the installer looks proper.
string(REPLACE ".wxs" "_license.rtf" _WIX_LICENSE_RTF "${WIX_OUTPUT_WXS}")
file(WRITE "${_WIX_LICENSE_RTF}"
"{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fswiss\\fcharset0 Arial;}}"
"{\\colortbl ;\\red0\\green0\\blue0;}"
"\\viewkind4\\uc1\\pard\\cf1\\f0\\fs22\\b ${GAME_NAME}\\b0\\par\\par"
"Built with the mini-mbm game engine.\\par\\par"
"\\b License (MIT)\\b0\\par\\par"
"Copyright (c) mini-mbm contributors\\par\\par"
"Permission is hereby granted, free of charge, to any person obtaining a copy "
"of this software and associated documentation files (the \\ldblquote Software\\rdblquote ), "
"to deal in the Software without restriction, including without limitation the rights "
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies "
"of the Software, and to permit persons to whom the Software is furnished to do so, "
"subject to the following conditions:\\par\\par"
"The above copyright notice and this permission notice shall be included in all "
"copies or substantial portions of the Software.\\par\\par"
"THE SOFTWARE IS PROVIDED \\ldblquote AS IS\\rdblquote , WITHOUT WARRANTY OF ANY KIND, "
"EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS "
"OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN "
"AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH "
"THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\\par}"
)

# ── Write .wxs ────────────────────────────────────────────────────────────────
file(WRITE "${WIX_OUTPUT_WXS}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!-- Generated by mini-mbm cmake make_wix.cmake - do not edit manually. -->
<Wix xmlns=\"http://schemas.microsoft.com/wix/2006/wi\">
    <Product Id=\"*\"
             UpgradeCode=\"{${WIX_UPGRADE_CODE}}\"
             Name=\"${GAME_NAME}\"
             Version=\"1.0.0\"
             Manufacturer=\"mini-mbm\"
             Language=\"1033\">
        <Package InstallerVersion=\"200\" Compressed=\"yes\" InstallScope=\"perUser\" />
        <MajorUpgrade DowngradeErrorMessage=\"A newer version of [ProductName] is already installed.\" />
        <MediaTemplate EmbedCab=\"yes\" />
${_WIX_ICON_XML}
        <Directory Id=\"TARGETDIR\" Name=\"SourceDir\">
            <Directory Id=\"LocalAppDataFolder\">
                <Directory Id=\"ProgramsFolder\" Name=\"Programs\">
                    <Directory Id=\"INSTALLDIR\" Name=\"${GAME_NAME_SAFE}\">
${WXS_BODY}                    </Directory>
                </Directory>
            </Directory>
            <Directory Id=\"ProgramMenuFolder\">
                <Directory Id=\"ProgramMenuDir\" Name=\"${GAME_NAME}\" />
            </Directory>
            <Directory Id=\"DesktopFolder\" />
        </Directory>

${_SHORTCUT_COMP}
        <Feature Id=\"ProductFeature\" Title=\"${GAME_NAME}\" Level=\"1\">
${_COMP_REFS}
        </Feature>

        <WixVariable Id=\"WixUILicenseRtf\" Value=\"${_WIX_LICENSE_RTF}\" />
        <UIRef Id=\"WixUI_Minimal\" />

    </Product>
</Wix>
")
message(STATUS "Generated WiX source: ${WIX_OUTPUT_WXS}")

# ── Build the MSI ─────────────────────────────────────────────────────────────
if(WIX_V4)
    execute_process(
        COMMAND "${WIX_V4}" build "${WIX_OUTPUT_WXS}"
            -o "${WIX_OUTPUT_MSI}"
            -ext WixToolset.UI.wixext
        WORKING_DIRECTORY "${WIN_GAMEDIR}"
        RESULT_VARIABLE _RES)
else()
    # WiX v3: candle → .wixobj, light → .msi
    set(_WIXOBJ "${CMAKE_BINARY_DIR}/@GAME_NAME_SAFE@.wixobj")
    execute_process(
        COMMAND "${WIX_CANDLE}" "${WIX_OUTPUT_WXS}"
            -o "${_WIXOBJ}"
            -ext WixUIExtension
        WORKING_DIRECTORY "${WIN_GAMEDIR}"
        RESULT_VARIABLE _RES1)
    if(_RES1 EQUAL 0)
        execute_process(
            COMMAND "${WIX_LIGHT}" "${_WIXOBJ}"
                -o "${WIX_OUTPUT_MSI}"
                -ext WixUIExtension
                -sice:ICE38
                -sice:ICE57
                -sice:ICE64
                -sw1076
            WORKING_DIRECTORY "${WIN_GAMEDIR}"
            RESULT_VARIABLE _RES)
    else()
        set(_RES ${_RES1})
    endif()
endif()

if(_RES EQUAL 0)
    message(STATUS "--- MSI ready: ${WIX_OUTPUT_MSI}")
else()
    message(FATAL_ERROR "WiX build failed with code ${_RES}")
endif()
]==]
            @ONLY)

        add_custom_target(msi
            DEPENDS mini-mbm
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/make_wix.cmake"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Building WiX MSI: ${GAME_NAME_SAFE}-windows.msi")

        # ── ZIP portable archive ──────────────────────────────────────────────
        # cmake -E tar with --format=zip produces a standard ZIP archive.
        # Pass a relative path so the zip root is GameName.GameDir/ (not the
        # full C:/build/... absolute path) — Windows Explorer requires this.
        add_custom_target(zip
            DEPENDS mini-mbm
            COMMAND ${CMAKE_COMMAND} -E tar cf
                "${CMAKE_BINARY_DIR}/${GAME_NAME_SAFE}-windows.zip"
                --format=zip
                "${GAME_NAME_SAFE}.GameDir"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Building ZIP: ${GAME_NAME_SAFE}-windows.zip")

        message(STATUS "After 'make': run  ${_WIN_GAMEDIR}/launch.bat")
        message(STATUS "Installer (opt.)   : make nsis  → ${GAME_NAME_SAFE}-windows-setup.exe")
        message(STATUS "MSI (opt.)         : make msi   → ${GAME_NAME_SAFE}-windows.msi")
        message(STATUS "ZIP (opt.)         : make zip   → ${GAME_NAME_SAFE}-windows.zip")

    endif() # GAME_ASSETS_DIR

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    if (USE_LUA)
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${SCENE_CPP}
                            ${LIB_MINIMBM_LUA_SRC}
                            ${LIB_MINIMBM_LUA_PLATFORM_SRC})
    else()
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${SCENE_CPP})
    endif()
    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    target_link_libraries(mini-mbm "-framework Cocoa")

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    # iOS static-link plugin definitions (same workaround as Android — dynamic loading is blocked)
    if(USE_LSQLITE3)
        add_definitions(-DUSE_LSQLITE3)
    endif()
    if(USE_BOX2D_LIQUID_FUN)
        add_definitions(-DUSE_BOX2D_LIQUID_FUN)
    endif()
    if(USE_BOX2D)
        add_definitions(-DUSE_BOX2D)
    endif()
    if(USE_BULLET3D)
        add_definitions(-DUSE_BULLET3D)
    endif()
    if(USE_IMGUI)
        add_definitions(-DUSE_IMGUI)
    endif()

    if (USE_LUA)
        add_executable(mini-mbm ${MAIN}
                            ${IOS_PLATFORM_SOURCES}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${LIB_MINIMBM_LUA_SRC}
                            ${LIB_MINIMBM_LUA_PLATFORM_SRC})
    else()
        add_executable(mini-mbm ${MAIN}
                            ${IOS_PLATFORM_SOURCES}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES})
    endif()
    # ── Per-game identity ─────────────────────────────────────────────────────
    # Override at configure time from your game repo's build script:
    #   -DGAME_BUNDLE_ID=com.yourcompany.yourgame
    #   -DGAME_NAME="Tower Defense"
    #   -DGAME_ASSETS_DIR=/path/to/tower-defense/assets
    if(NOT DEFINED GAME_BUNDLE_ID)
        set(GAME_BUNDLE_ID "com.mini.mbm.mini-mbm")
    endif()
    if(NOT DEFINED GAME_NAME)
        set(GAME_NAME "mini-mbm")
    endif()
    set(IOS_BUNDLE_ID "${GAME_BUNDLE_ID}")
    set(IOS_APP_NAME  "${GAME_NAME}")
    # Write Info.plist into the source tree so Xcode's navigator can find it.
    # (Placing it in CMAKE_BINARY_DIR causes the Xcode navigator to show it red,
    #  even though the build still works — this avoids that cosmetic issue.)
    configure_file(
        ${CMAKE_SOURCE_DIR}/platform-ios/Info.plist.in
        ${CMAKE_SOURCE_DIR}/platform-ios/Info.plist
        @ONLY)
    message(STATUS "iOS bundle ID : ${GAME_BUNDLE_ID}")
    message(STATUS "iOS app name  : ${GAME_NAME}")

    set_target_properties(mini-mbm PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/platform-ios/Info.plist
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${GAME_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        # Required for "Distribute App → App Store Connect" to appear in the
        # Xcode Organizer. CMake defaults INSTALL_PATH to "" for all targets;
        # Xcode interprets that as "not an installable app" and only shows
        # "Distribute Content". Setting it to $(LOCAL_APPS_DIR) tells Xcode
        # this is a real .app archive, unlocking the App Store upload path.
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
        XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
        # App Store Connect requires a separate .dSYM bundle in the archive.
        # CMake defaults DEBUG_INFORMATION_FORMAT to "dwarf" (embedded) and sets
        # GCC_GENERATE_DEBUGGING_SYMBOLS=NO for Release builds. Both must be
        # overridden: the compiler must emit DWARF (-g), then dsymutil extracts
        # it into a separate .dSYM. Without GCC_GENERATE_DEBUGGING_SYMBOLS=YES
        # the compiler produces no debug info and dsymutil has nothing to work
        # with, leaving the archive's dSYMs/ folder empty.
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
        # CMake sets CMAKE_RUNTIME_OUTPUT_DIRECTORY globally to mini-mbm/bin/…
        # which Xcode translates into CONFIGURATION_BUILD_DIR for every target.
        # During archive, Xcode copies the .dSYM into the archive only when it
        # sits beside the .app in the archive intermediates SYMROOT — not in the
        # custom bin/ path. Resetting CONFIGURATION_BUILD_DIR to the Xcode default
        # ($(SYMROOT)/$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)) ensures the
        # dSYM lands where Xcode expects it and gets included in the .xcarchive.
        XCODE_ATTRIBUTE_CONFIGURATION_BUILD_DIR "$(SYMROOT)/$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)")
    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    target_link_libraries(mini-mbm "-framework UIKit")
    target_link_libraries(mini-mbm "-framework Metal")
    target_link_libraries(mini-mbm "-framework AVFoundation")
    target_link_libraries(mini-mbm "-framework AudioToolbox")
    target_link_libraries(mini-mbm "-framework Foundation")
    # UniformTypeIdentifiers is used by MbmCommandBridge.mm (UTType / file picker).
    # iOS 14+ only — needs to be explicit so CMake/Xcode includes it in the link phase.
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        target_link_libraries(mini-mbm "-framework UniformTypeIdentifiers")
    endif()

    # ── Game assets ───────────────────────────────────────────────────────────
    # With -G Xcode: files are added as target sources with MACOSX_PACKAGE_LOCATION
    # so they appear in the Xcode project navigator (editable like in Android Studio)
    # and Xcode copies them into the .app bundle under assets/ automatically.
    # With make: fall back to a POST_BUILD copy_directory (no navigator needed).
    # NOTE: adding new files to GAME_ASSETS_DIR requires re-running cmake so that
    # CMake's GLOB_RECURSE picks them up — the build itself does not need a clean.
    if(DEFINED GAME_ASSETS_DIR)
        if(NOT IS_DIRECTORY "${GAME_ASSETS_DIR}")
            message(WARNING "GAME_ASSETS_DIR '${GAME_ASSETS_DIR}' does not exist or is not a directory — no assets will be bundled. Check the path (tilde ~ is not expanded inside double-quoted cmake -D values).")
        endif()
        message(STATUS "iOS assets dir: ${GAME_ASSETS_DIR}")
        if(CMAKE_GENERATOR STREQUAL "Xcode")
            # Collect every file (not directories) under GAME_ASSETS_DIR
            file(GLOB_RECURSE _GAME_ASSET_FILES
                LIST_DIRECTORIES FALSE
                "${GAME_ASSETS_DIR}/*")
            foreach(_asset_file ${_GAME_ASSET_FILES})
                # Compute path relative to GAME_ASSETS_DIR, e.g. "image/hero.png"
                file(RELATIVE_PATH _rel_path "${GAME_ASSETS_DIR}" "${_asset_file}")
                # Keep only the directory component, e.g. "image"
                get_filename_component(_rel_dir "${_rel_path}" DIRECTORY)
                if(_rel_dir)
                    set(_pkg_loc "assets/${_rel_dir}")
                else()
                    set(_pkg_loc "assets")
                endif()
                # Tell Xcode: this file is a resource to be placed in assets/<subdir>/
                set_source_files_properties("${_asset_file}" PROPERTIES
                    MACOSX_PACKAGE_LOCATION "${_pkg_loc}")
            endforeach()
            target_sources(mini-mbm PRIVATE ${_GAME_ASSET_FILES})
            # Mirror the source directory tree in the Xcode navigator
            source_group(TREE "${GAME_ASSETS_DIR}" PREFIX "assets" FILES ${_GAME_ASSET_FILES})
        else()
            # make generator: simple directory copy post-build
            add_custom_command(TARGET mini-mbm POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${GAME_ASSETS_DIR}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:mini-mbm>/assets"
                COMMENT "Copying game assets from ${GAME_ASSETS_DIR}")
        endif()
    endif()

    # ── App icon ──────────────────────────────────────────────────────────────
    # CMake's Xcode generator creates a PBXFileReference for .xcassets
    # directories but only places them in a build phase when they are in the
    # SOURCE tree.  Paths under CMAKE_BINARY_DIR are silently omitted from all
    # build phases, so Xcode's "Compile Asset Catalogs" step never fires and the
    # icon is never compiled into the app bundle.
    #
    # Solution: always keep Assets.xcassets in platform-ios/ (source tree).
    # At cmake configure time we populate AppIcon.appiconset/ inside it from the
    # game-supplied icon, so it is automatically compiled by Xcode before code
    # signing — exactly as in a hand-crafted Xcode project.
    #
    # Flags:
    #   -DGAME_ICON_PNG=/path/to/icon-1024.png   — preferred: CMake flattens alpha
    #   -DGAME_ICON_DIR=/path/to/Assets.xcassets  — bring your own xcassets

    set(_IOS_ASSETS_DIR  "${CMAKE_SOURCE_DIR}/platform-ios/Assets.xcassets")
    set(_IOS_APPICONSET_DIR "${_IOS_ASSETS_DIR}/AppIcon.appiconset")

    # Ensure the catalog container exists (no icon entries by default).
    file(WRITE "${_IOS_ASSETS_DIR}/Contents.json"
        "{\n  \"info\" : { \"author\" : \"mini-mbm\", \"version\" : 1 }\n}\n")

    if(DEFINED GAME_ICON_PNG)
        if(NOT EXISTS "${GAME_ICON_PNG}")
            message(WARNING "GAME_ICON_PNG '${GAME_ICON_PNG}' does not exist — icon will not be bundled.")
        else()
            # iOS requires fully opaque app icons — alpha transparency causes the
            # icon to render as black or invisible on the home screen.  The static
            # script platform-ios/flatten_icon.swift (CoreGraphics/ImageIO) composites
            # the source PNG onto white and writes an RGB PNG to the binary dir.
            get_filename_component(_ICON_BASENAME "${GAME_ICON_PNG}" NAME_WE)
            set(_ICON_FLAT_PNG "${CMAKE_BINARY_DIR}/${_ICON_BASENAME}_flat.png")
            set(_FLATTEN_SWIFT "${CMAKE_SOURCE_DIR}/platform-ios/flatten_icon.swift")
            execute_process(
                COMMAND swift "${_FLATTEN_SWIFT}" "${GAME_ICON_PNG}" "${_ICON_FLAT_PNG}"
                RESULT_VARIABLE _SWIFT_RESULT
                OUTPUT_QUIET ERROR_VARIABLE _SWIFT_ERR)
            if(NOT _SWIFT_RESULT EQUAL 0)
                set(_ICON_FLAT_PNG "${GAME_ICON_PNG}")
                message(WARNING "Icon alpha-strip failed: ${_SWIFT_ERR}\nUsing original icon (transparent areas will appear black on the home screen).")
            else()
                message(STATUS "iOS icon (alpha composited onto white): ${_ICON_FLAT_PNG}")
            endif()

            # Populate AppIcon.appiconset inside the source-tree Assets.xcassets.
            get_filename_component(_ICON_FILENAME "${_ICON_FLAT_PNG}" NAME)
            set(_ICON_JSON
"{\n  \"images\" : [\n    {\n      \"filename\" : \"${_ICON_FILENAME}\",\n      \"idiom\" : \"universal\",\n      \"platform\" : \"ios\",\n      \"size\" : \"1024x1024\"\n    }\n  ],\n  \"info\" : {\n    \"author\" : \"mini-mbm\",\n    \"version\" : 1\n  }\n}\n")
            file(WRITE "${_IOS_APPICONSET_DIR}/Contents.json" "${_ICON_JSON}")
            configure_file("${_ICON_FLAT_PNG}" "${_IOS_APPICONSET_DIR}/${_ICON_FILENAME}" COPYONLY)
            message(STATUS "iOS icon PNG     : ${GAME_ICON_PNG}")
            message(STATUS "iOS icon xcassets: ${_IOS_ASSETS_DIR}")
            set(_HAS_ICON TRUE)
        endif()
    endif()

    if(DEFINED GAME_ICON_DIR)
        if(NOT IS_DIRECTORY "${GAME_ICON_DIR}")
            message(WARNING "GAME_ICON_DIR '${GAME_ICON_DIR}' is not a directory — icon will not be bundled.")
        else()
            # Copy the AppIcon.appiconset from the caller's xcassets into the
            # source-tree Assets.xcassets so Xcode can compile it normally.
            set(_SRC_APPICONSET "${GAME_ICON_DIR}/AppIcon.appiconset")
            if(IS_DIRECTORY "${_SRC_APPICONSET}")
                file(COPY "${_SRC_APPICONSET}" DESTINATION "${_IOS_ASSETS_DIR}")
                message(STATUS "iOS icon dir     : ${GAME_ICON_DIR}")
                set(_HAS_ICON TRUE)
            else()
                message(WARNING "GAME_ICON_DIR '${GAME_ICON_DIR}' has no AppIcon.appiconset — icon will not be bundled.")
            endif()
        endif()
    endif()

    # Make Assets.xcassets visible in Xcode's Project Navigator.
    # target_sources with a folder.assetcatalog path creates a PBXFileReference
    # (sourceTree = SOURCE_ROOT) so the catalog appears under platform-ios/ in
    # the navigator and can be browsed with Xcode's Asset Catalog editor.
    # It does NOT assign a build phase — compilation is handled by POST_BUILD.
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        target_sources(mini-mbm PRIVATE "${_IOS_ASSETS_DIR}")
    endif()

    # Compile the asset catalog and merge the icon info into Info.plist.
    # POST_BUILD fires BEFORE Xcode's code-signing step, so the bundle is
    # correctly signed after actool writes Assets.car + the partial Info.plist.
    if(_HAS_ICON)
        if(CMAKE_GENERATOR STREQUAL "Xcode")
            set(_ACTOOL_SCRIPT "${CMAKE_BINARY_DIR}/actool_icon.sh")
            # Embed the source-tree xcassets path so the script is self-contained.
            # Xcode env vars used:
            #   $BUILT_PRODUCTS_DIR — directory holding the .app in DerivedData
            #   $WRAPPER_NAME       — "mini-mbm.app"
            #   $PLATFORM_NAME      — "iphoneos" or "iphonesimulator"
            #   $IPHONEOS_DEPLOYMENT_TARGET — project's min-OS setting
            file(WRITE "${_ACTOOL_SCRIPT}"
"#!/bin/sh
set -e
PARTIAL=\"$(mktemp -t actool_partial).plist\"
xcrun actool \\
    --compile \"$BUILT_PRODUCTS_DIR/$WRAPPER_NAME\" \\
    --platform \"$PLATFORM_NAME\" \\
    --minimum-deployment-target \"$IPHONEOS_DEPLOYMENT_TARGET\" \\
    --app-icon AppIcon \\
    --output-partial-info-plist \"$PARTIAL\" \\
    \"${_IOS_ASSETS_DIR}\"
# Merge CFBundleIcons into the bundle Info.plist so Springboard finds the icon.
/usr/libexec/PlistBuddy -c \"Merge $PARTIAL\" \\
    \"$BUILT_PRODUCTS_DIR/$WRAPPER_NAME/Info.plist\" 2>/dev/null || true
rm -f \"$PARTIAL\"
")
            execute_process(COMMAND chmod +x "${_ACTOOL_SCRIPT}")
            add_custom_command(TARGET mini-mbm POST_BUILD
                COMMAND "${_ACTOOL_SCRIPT}"
                COMMENT "Compiling app icon asset catalog (actool)"
                VERBATIM)
            set_target_properties(mini-mbm PROPERTIES
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
        else()
            # make generator
            add_custom_command(TARGET mini-mbm POST_BUILD
                COMMAND xcrun actool
                    --compile "$<TARGET_BUNDLE_CONTENT_DIR:mini-mbm>"
                    --platform iphoneos
                    --minimum-deployment-target 14.0
                    --app-icon AppIcon
                    --output-partial-info-plist /dev/null
                    "${_IOS_ASSETS_DIR}"
                COMMENT "Compiling app icon asset catalog (actool)"
                VERBATIM)
        endif()
    endif()

else()
    message(SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM ${ENGINE_TARGET_PLATFORM}")
endif()

if (USE_LUA)
    target_link_libraries(mini-mbm ${LUA_LIBRARY})
    target_link_libraries(mini-mbm ${PLUGIN_HELPER_LIBRARY})
endif()

# plugins for android (https://developer.android.com/about/versions/nougat/android-7.0-changes.html#ndk) will be linked (workaround)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    if(USE_LSQLITE3)
        message(STATUS "USE_LSQLITE3 is enabled, ${SQLITE3_LUA_LIBRARY} for ANDROID will be linked as workaround")
        target_link_libraries(mini-mbm ${SQLITE3_LUA_LIBRARY})
    endif()

    if(USE_BOX2D)
        message(STATUS "USE_BOX2D is enabled, ${BOX2D_LUA_LIBRARY} for ANDROID will be linked as workaround")
        target_link_libraries(mini-mbm ${BOX2D_LUA_LIBRARY})
    endif()

    if(USE_BOX2D_LIQUID_FUN)
        message(STATUS "USE_BOX2D_LIQUID_FUN is enabled, ${BOX2D_LIQUID_FUN_LUA_LIBRARY} for ANDROID will be linked as workaround")
        target_link_libraries(mini-mbm ${BOX2D_LIQUID_FUN_LUA_LIBRARY})
    endif()

    if(USE_BULLET3D)
        message(STATUS "USE_BULLET3D is enabled, ${BULLET_3D_LIBRARY} for ANDROID will be linked as workaround")
        target_link_libraries(mini-mbm ${BULLET_3D_LIBRARY})
    endif()

    if(USE_IMGUI)
        message(STATUS "USE_IMGUI is enabled, ${IMGUI_LIBRARY} for ANDROID will be linked as workaround")
        target_link_libraries(mini-mbm ${IMGUI_LIBRARY})
    endif()

endif()

# plugins for iOS must also be statically linked (App Store sandbox forbids user-space dylibs)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")

    if(USE_LSQLITE3)
        message(STATUS "USE_LSQLITE3 is enabled — linking statically for iOS")
        target_link_libraries(mini-mbm ${SQLITE3_LUA_LIBRARY})
    endif()

    if(USE_BOX2D)
        message(STATUS "USE_BOX2D is enabled — linking statically for iOS")
        target_link_libraries(mini-mbm ${BOX2D_LUA_LIBRARY})
    endif()

    if(USE_BOX2D_LIQUID_FUN)
        message(STATUS "USE_BOX2D_LIQUID_FUN is enabled — linking statically for iOS")
        target_link_libraries(mini-mbm ${BOX2D_LIQUID_FUN_LUA_LIBRARY})
    endif()

    if(USE_BULLET3D)
        message(STATUS "USE_BULLET3D is enabled — linking statically for iOS")
        target_link_libraries(mini-mbm ${BULLET_3D_LIBRARY})
    endif()

    if(USE_IMGUI)
        message(STATUS "USE_IMGUI is enabled — linking statically for iOS")
        target_link_libraries(mini-mbm ${IMGUI_LIBRARY})
    endif()

endif()

get_filename_component(PATH_BIN
                       "${FOLDER_BIN}/${FOLDER_ARCHITECTURE}"
                       ABSOLUTE)
get_filename_component(PATH_LUA
                       "${ROOT_FOLDER}/lua-scripts"
                       ABSOLUTE)
get_filename_component(PATH_LUA_EDITOR
                       "${ROOT_FOLDER}/editor"
                       ABSOLUTE)
# Editor language module (required by all editors for EN/PT-BR support)
if(NOT EXISTS "${PATH_LUA_EDITOR}/lang/language.lua")
    message(WARNING "editor/lang/language.lua not found - editor language support will fail")
endif()




if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
    set(REDIRECT_SHELL                    "'>>'")
    if(USE_LUA)
        add_custom_target(
        shortcuts ALL
        DEPENDS mini-mbm
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "binaries created at ${PATH_BIN}"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "Creating alias..."
        COMMAND cmake -E echo "alias for scene_editor"
        COMMAND cmake -E echo "alias for sprite_maker"
        COMMAND cmake -E echo "alias for font_maker"
        COMMAND cmake -E echo "alias for mesh_debug"
        COMMAND cmake -E echo "alias for shader_editor"
        COMMAND cmake -E echo "alias for particle_editor"
        COMMAND cmake -E echo "alias for physic_editor"
        COMMAND cmake -E echo "alias for mini-mbm"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "alias lua='${PATH_BIN}/lua-${LUA_LIB_VERSION}.exe'" > ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias scene_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/scene_editor2d.lua  --nosplash --name scene_editor2d'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias sprite_maker='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/sprite_maker.lua  --nosplash --name sprite_maker'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias font_maker='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/font_maker.lua  --nosplash --name font_maker'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias mesh_debug='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/mesh_debug.lua  --nosplash --name mesh_debug'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias shader_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/shader_editor.lua  --nosplash --name shader_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias particle_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/particle_editor.lua  --nosplash --name particle_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias physic_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/physic_editor.lua  --nosplash --name physic_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias asset_packager='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/asset_packager.lua  --nosplash --name asset_packager'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias tilemap_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/tilemap_editor.lua  --nosplash --name tilemap_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias texture_packer='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/texture_packer.lua  --nosplash --name texture_packer'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias minimbm='${PATH_BIN}/mini-mbm '" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_PATH='\"?;?.lua;${PATH_BIN}/?.lua\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_CPATH='\"?;?.so;${PATH_BIN}/?.so\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LD_LIBRARY_PATH=${PATH_BIN}:/usr/local/lib:/usr/lib/x86_64-linux-gnu/" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "aliases created at ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "add ${PATH_BIN}/shortcuts.sh at ~/.bashrc to make your shortcut"
        COMMAND cmake -E echo "do:"
        COMMAND cmake -E echo "chmod 755 ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "echo \"source ${PATH_BIN}/shortcuts.sh \"  ${REDIRECT_SHELL} ~/.bashrc"
        COMMAND cmake -E echo ""
        COMMAND cmake -E echo "---------------------------------------------"
        )
    else()
    add_custom_target(
        shortcuts ALL
        DEPENDS mini-mbm
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "binaries created at ${PATH_BIN}"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "Creating alias..."
        COMMAND cmake -E echo "alias for mini-mbm"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "alias minimbm='${PATH_BIN}/mini-mbm'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_PATH='\"?;?.lua;${PATH_BIN}/?.lua\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_CPATH='\"?;?.so;${PATH_BIN}/?.so\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LD_LIBRARY_PATH=${PATH_BIN}:/usr/local/lib:/usr/lib/x86_64-linux-gnu/" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "aliases created at ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "add ${PATH_BIN}/shortcuts.sh at ~/.bashrc to make your shortcut"
        COMMAND cmake -E echo "do:"
        COMMAND cmake -E echo "chmod 755 ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "echo \"source ${PATH_BIN}/shortcuts.sh \"  ${REDIRECT_SHELL} ~/.bashrc"
        COMMAND cmake -E echo ""
        COMMAND cmake -E echo "---------------------------------------------")
    endif()
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    set(REDIRECT_SHELL                    "'>>'")
    if(USE_LUA)
        add_custom_target(
        shortcuts ALL
        DEPENDS mini-mbm
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "binaries created at ${PATH_BIN}"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "Creating alias..."
        COMMAND cmake -E echo "alias for scene_editor"
        COMMAND cmake -E echo "alias for sprite_maker"
        COMMAND cmake -E echo "alias for font_maker"
        COMMAND cmake -E echo "alias for mesh_debug"
        COMMAND cmake -E echo "alias for shader_editor"
        COMMAND cmake -E echo "alias for particle_editor"
        COMMAND cmake -E echo "alias for physic_editor"
        COMMAND cmake -E echo "alias for mini-mbm"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "alias lua='${PATH_BIN}/lua-${LUA_LIB_VERSION}'" > ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias scene_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/scene_editor2d.lua -w 1470 -h 900 --nosplash --name scene_editor2d'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias sprite_maker='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/sprite_maker.lua -w 1470 -h 900 --nosplash --name sprite_maker'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias font_maker='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/font_maker.lua  -w 1470 -h 900 --nosplash --name font_maker'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias mesh_debug='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/mesh_debug.lua -w 1470 -h 900 --nosplash --name mesh_debug'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias shader_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/shader_editor.lua -w 1470 -h 900 --nosplash --name shader_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias particle_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/particle_editor.lua  -w 1470 -h 900 --nosplash --name particle_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias physic_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/physic_editor.lua  -w 1470 -h 900 --nosplash --name physic_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias asset_packager='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/asset_packager.lua  -w 1470 -h 900 --nosplash --name asset_packager'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias tilemap_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/tilemap_editor.lua  -w 1470 -h 900 --nosplash --name tilemap_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias texture_packer='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/texture_packer.lua  -w 1470 -h 900 --nosplash --name texture_packer'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias minimbm='${PATH_BIN}/mini-mbm '" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_PATH='\"?;?.lua;${PATH_BIN}/?.lua\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_CPATH='\"?;?.dylib;${PATH_BIN}/?.dylib\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export DYLD_LIBRARY_PATH=${PATH_BIN}:/usr/local/lib" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "aliases created at ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "add ${PATH_BIN}/shortcuts.sh at ~/.zshrc to make your shortcut"
        COMMAND cmake -E echo "do:"
        COMMAND cmake -E echo "chmod 755 ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "echo \"source ${PATH_BIN}/shortcuts.sh \"  ${REDIRECT_SHELL} ~/.zshrc"
        COMMAND cmake -E echo ""
        COMMAND cmake -E echo "---------------------------------------------"
        )
    else()
        add_custom_target(
        shortcuts ALL
        DEPENDS mini-mbm
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "binaries created at ${PATH_BIN}"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "Creating alias..."
        COMMAND cmake -E echo "alias for mini-mbm"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "alias minimbm='${PATH_BIN}/mini-mbm'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_PATH='\"?;?.lua;${PATH_BIN}/?.lua\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_CPATH='\"?;?.dylib;${PATH_BIN}/?.dylib\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export DYLD_LIBRARY_PATH=${PATH_BIN}:/usr/local/lib" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "aliases created at ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "add ${PATH_BIN}/shortcuts.sh at ~/.zshrc to make your shortcut"
        COMMAND cmake -E echo "do:"
        COMMAND cmake -E echo "chmod 755 ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "echo \"source ${PATH_BIN}/shortcuts.sh \"  ${REDIRECT_SHELL} ~/.zshrc"
        COMMAND cmake -E echo ""
        COMMAND cmake -E echo "---------------------------------------------")
    endif()
endif()

