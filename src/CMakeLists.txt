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
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${GAME_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
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
            # icon to render as black or invisible on the home screen.  Use Swift
            # (CoreGraphics/ImageIO, always available with Xcode) to composite the
            # source PNG onto a white background.  The result is written to the
            # binary dir with a "_flat" suffix; the original file is never modified.
            get_filename_component(_ICON_BASENAME "${GAME_ICON_PNG}" NAME_WE)
            set(_ICON_FLAT_PNG "${CMAKE_BINARY_DIR}/${_ICON_BASENAME}_flat.png")
            set(_FLATTEN_SWIFT "${CMAKE_BINARY_DIR}/flatten_icon.swift")
            file(WRITE "${_FLATTEN_SWIFT}"
"import CoreGraphics
import ImageIO
import Foundation

let srcURL = URL(fileURLWithPath: \"${GAME_ICON_PNG}\") as CFURL
let dstURL = URL(fileURLWithPath: \"${_ICON_FLAT_PNG}\") as CFURL

guard let src = CGImageSourceCreateWithURL(srcURL, nil),
      let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else {
    print(\"flatten_icon: cannot open source PNG\"); exit(1)
}

let w = img.width, h = img.height
let cs = CGColorSpaceCreateDeviceRGB()
guard let ctx = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8,
                          bytesPerRow: 0, space: cs,
                          bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue) else {
    print(\"flatten_icon: cannot create CGContext\"); exit(1)
}
ctx.setFillColor(CGColor(colorSpace: cs, components: [1.0, 1.0, 1.0, 1.0])!)
ctx.fill(CGRect(x: 0, y: 0, width: Double(w), height: Double(h)))
ctx.draw(img, in: CGRect(x: 0, y: 0, width: Double(w), height: Double(h)))

guard let outImg = ctx.makeImage() else {
    print(\"flatten_icon: cannot create output image\"); exit(1)
}
guard let dest = CGImageDestinationCreateWithURL(dstURL, \"public.png\" as CFString, 1, nil) else {
    print(\"flatten_icon: cannot create image destination\"); exit(1)
}
CGImageDestinationAddImage(dest, outImg, nil)
guard CGImageDestinationFinalize(dest) else {
    print(\"flatten_icon: finalize failed\"); exit(1)
}
")
            execute_process(
                COMMAND swift "${_FLATTEN_SWIFT}"
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

    # Register the source-tree xcassets with the target.
    # CMake's Xcode generator cannot place .xcassets directories into
    # PBXResourcesBuildPhase (it skips directory paths for all build phases).
    # The workaround is a POST_BUILD shell script that runs actool manually.
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
PARTIAL=\"$(mktemp /tmp/actool_partial_XXXXXX.plist)\"
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

