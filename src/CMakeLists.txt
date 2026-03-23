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
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main.cpp)
        set(LIB_MINIMBM_LUA_PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib-Windows.cpp)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(GLES_INCLUDE     ${THIRD_PARTY}/gles)
        endif()
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-android/main-lua.cpp)
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
        set_source_files_properties(${LIB_MINIMBM_LUA_PLATFORM_SRC} ${IOS_PLATFORM_SOURCES}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    else()
        message(SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM ${ENGINE_TARGET_PLATFORM}")
    endif()

else()

    message(STATUS " Using pure CPP...")
    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-linux/main.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-linux/my-scene.cpp)
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main.cpp)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(GLES_INCLUDE     ${THIRD_PARTY}/gles)
        endif()
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-android/main.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-android/scene-1.cpp)
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
include_directories(${OPENGLES2_INCLUDE_DIR} 
                    ${INCLUDE}
                    #${GLES_INCLUDE}
                    ${RENDER_INCLUDES} 
                    ${CORE_OPENGLES_INCLUDES} 
                    ${THIRD_PARTY}
                    ${LUA_WRAP}
                    ${LUA_INCLUDE}
                    ${PLUGINS}
                    ${LIB_MINIMBM_LUA_HEADER})

 
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

    add_library(mini-mbm  SHARED  ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${SCENE_CPP})

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})

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

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")

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
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${GAME_BUNDLE_ID}")
    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    target_link_libraries(mini-mbm "-framework UIKit")
    target_link_libraries(mini-mbm "-framework Metal")
    target_link_libraries(mini-mbm "-framework AVFoundation")
    target_link_libraries(mini-mbm "-framework AudioToolbox")
    target_link_libraries(mini-mbm "-framework Foundation")

    # ── Game assets ───────────────────────────────────────────────────────────
    # With -G Xcode: files are added as target sources with MACOSX_PACKAGE_LOCATION
    # so they appear in the Xcode project navigator (editable like in Android Studio)
    # and Xcode copies them into the .app bundle under assets/ automatically.
    # With make: fall back to a POST_BUILD copy_directory (no navigator needed).
    # NOTE: adding new files to GAME_ASSETS_DIR requires re-running cmake so that
    # CMake's GLOB_RECURSE picks them up — the build itself does not need a clean.
    if(DEFINED GAME_ASSETS_DIR)
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

