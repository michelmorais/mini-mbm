cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(core_mbm)

set(SRC_INCLUDES               "../../include")
set(CORE_OPENGLES_SRC          ${CMAKE_CURRENT_SOURCE_DIR})
set(RENDER_SRC                 "../render")
set(THIRD_PARTY                "../../third-party")
set(RENDER_INCLUDE             ${SRC_INCLUDES}/render)
set(CORE_OPENGLES_INCLUDE      ${SRC_INCLUDES}/core_mbm)
set(BOX2D_LIQUID_FUN_WRAP_SRC  box-2d-liquid-fun-wrap)
set(BULLET3d_WRAP_INCLUDE      ${SRC_INCLUDES}/bullet-3d-wrap)
set(BULLET3d_WRAP_SRC          bullet-3d-wrap)
set(CORE_OPENGLES_MINIZ        ${CORE_OPENGLES_SRC}/miniz)
set(CORE_OPENGLES_PLATFORM     ${CORE_OPENGLES_SRC}/platform)
set(STB_DIR                    ${THIRD_PARTY}/stb)
set(MINIZ_DIR                  ${THIRD_PARTY}/miniz)
set(LODE_PNG_DIR               ${THIRD_PARTY}/lodepng)
set(BOX2D_LIQUID_FUN_INC       ${THIRD_PARTY}/box2d-liquid-fun-${BOX2D_LIQUID_FUN_LIB_VERSION})
set(BULLET_3D_INC              ${THIRD_PARTY}/bullet-${BULLET_LIB_VERSION})
set(TINYFILEDIALOGS_DIR        ${THIRD_PARTY}/tinyfiledialogs)
set(PORT_AUDIO_SRC             ${THIRD_PARTY}/portaudio/src)
set(PORT_AUDIO_INCLUDE         ${THIRD_PARTY}/portaudio/include)
set(WAVE_SRC                   ${THIRD_PARTY}/wave)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-misleading-indentation")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-misleading-indentation")
endif()


#Bring the headers, such as xxx.h into the project
include_directories(${CORE_OPENGLES_INCLUDE} 
                    #${GLES_INCLUDE}
                    ${RENDER_INCLUDE} 
                    ${CORE_OPENGLES_MINIZ} 
                    ${CORE_OPENGLES_PLATFORM} 
                    ${SRC_INCLUDES} 
                    ${STB_DIR} 
                    ${THIRD_PARTY} 
                    ${BOX2D_LIQUID_FUN_WRAP_INCLUDE} 
                    ${BOX2D_LIQUID_FUN_INC}
                    ${BULLET_3D_INC}
                    ${PORT_AUDIO_INCLUDE}
                    ${WAVE_SRC})

 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES                   "${CORE_OPENGLES_SRC}/*.cpp")
file(GLOB_RECURSE RENDER_SOURCES            "${RENDER_SRC}/*.cpp")
file(GLOB_RECURSE MINI_Z_SOURCES            "${MINIZ_DIR}/*.c")
file(GLOB_RECURSE STB_SOURCES               "${STB_DIR}/*.c")
file(GLOB_RECURSE LODE_PNG_SOURCES          "${LODE_PNG_DIR}/*.cpp")

# Windows-only: plusWindows UI library and HID joystick implementation.
# These live under include/ (not src/) so the main SOURCES glob misses them.
set(PLUS_WINDOWS_SOURCES "")
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
    set(PLUS_WINDOWS_SOURCES
        "${SRC_INCLUDES}/plusWindows/plusWindows.cpp"
        "${SRC_INCLUDES}/plusWindows/defaultThemePlusWindows.cpp"
        "${SRC_INCLUDES}/joystick-win32/joystick-win32.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/win32-platform.cpp")
    include_directories("${SRC_INCLUDES}/plusWindows"
                        "${SRC_INCLUDES}/joystick-win32")
    # plusWindows.cpp uses SetWindowLong/GetWindowLong with pointer-sized values,
    # written for 32-bit MSVC where long == pointer size.  On 64-bit MinGW the
    # casts lose precision; -fpermissive downgrades those errors to warnings.
    set_source_files_properties(
        "${SRC_INCLUDES}/plusWindows/plusWindows.cpp"
        PROPERTIES COMPILE_FLAGS "-fpermissive")
endif()

# Suppress warnings in third-party sources that we don't own and don't want to patch.
if(APPLE)
    set_source_files_properties(${LODE_PNG_SOURCES} PROPERTIES
        COMPILE_FLAGS "-Wno-shorten-64-to-32")
    set_source_files_properties(${MINI_Z_SOURCES} PROPERTIES
        COMPILE_FLAGS "-Wno-#warnings")
    set_source_files_properties(${STB_SOURCES} PROPERTIES
        COMPILE_FLAGS "-Wno-tautological-pointer-compare")
endif()

if (${AUDIO} STREQUAL "audiere")
    add_definitions(-DAUDIO_ENGINE_AUDIERE)
elseif (${AUDIO} STREQUAL "dsound")
    add_definitions(-DAUDIO_ENGINE_DIRECT_SOUND_8)
elseif (${AUDIO} STREQUAL "portaudio")
    #define REVERSE_ENDIANISM if the endianism of the host platform is not Intel
    # add_definitions(-DREVERSE_ENDIANISM)
    file(GLOB_RECURSE PORT_AUDIO_SOURCES        "${PORT_AUDIO_SRC}/*.cpp")
    file(GLOB_RECURSE WAVE_SOURCES              "${WAVE_SRC}/*.cpp")
    find_path(HEADER_PORT_AUDIO portaudio.h)
    if(NOT HEADER_PORT_AUDIO)
        message( SEND_ERROR "portaudio.h not found. try\n apt-get install portaudio19-dev")
    endif()

    find_library(PORT_AUDIO_LIBRARY portaudio)
    if(NOT PORT_AUDIO_LIBRARY)
        message( SEND_ERROR "portaudio not found. try\n apt-get install portaudio19-dev")
    endif()
    include_directories(${HEADER_PORT_AUDIO})
    add_definitions(-DAUDIO_ENGINE_PORT_AUDIO)
elseif (${AUDIO} STREQUAL "avfoundation")
    add_definitions(-DAUDIO_ENGINE_AVFOUNDATION)
elseif (${AUDIO} STREQUAL "opensl")
    add_definitions(-DAUDIO_ENGINE_ANDROID_OPENSL)
elseif( NOT ${AUDIO} STREQUAL "none")
    message(SEND_ERROR " Unknow AUDIO choice: ${AUDIO}")
endif()

if (${AUDIO} STREQUAL "dsound" AND ${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
    target_link_libraries(core_mbm dsound)
endif()

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
	#remove common-jni ANDROID stuff file
    file(GLOB_RECURSE TINYFILEDIALOGS_SOURCES   "${TINYFILEDIALOGS_DIR}/*.c")
endif()


if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

    #Opengl
    include(FindOpenGLES2)
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    ####

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    #Opengl
    include(FindOpenGLES2)
    find_package(ndk-stl REQUIRED)#depend on cmake-modules/Findndk-stl.cmake 

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")

    # DirectX9 needs no find_* step — its headers ship with MinGW/Windows SDK.
    # GLES import libs are referenced directly in target_link_libraries below.
    # Threads: needed by Lua and the engine on MinGW.
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")

    if(USE_OPENGL_ES)
        #Opengl
        include(FindOpenGLES2)
    endif()
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    ####

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")

    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    ####

else()

    message( SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM=${ENGINE_TARGET_PLATFORM} in src/CMakeLists.txt")

endif()


# Gather Objective-C++ Metal sources (MacOs/iOS + USE_METAL only)
set(METAL_MM_SOURCES "")
set(METAL_RENDER_MM_SOURCES "")
if ((${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "iOS") AND USE_METAL)
    file(GLOB_RECURSE METAL_MM_SOURCES "${CORE_OPENGLES_SRC}/*.mm")
    file(GLOB_RECURSE METAL_RENDER_MM_SOURCES "${RENDER_SRC}/*.mm")
    # Enable Automatic Reference Counting for all Objective-C++ Metal sources.
    # Without ARC, Metal id<> objects returned by factory methods would be
    # autoreleased and freed before we use them across frame boundaries.
    if (METAL_MM_SOURCES)
        set_source_files_properties(${METAL_MM_SOURCES}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    endif()
    if (METAL_RENDER_MM_SOURCES)
        set_source_files_properties(${METAL_RENDER_MM_SOURCES}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    endif()
endif()

# For Apple builds that don't use Metal (e.g. USE_DUMMY_BACK_END_ENGINE), the
# dialog-util-macos.mm file is not picked up by the Metal .mm glob above.
# Add it explicitly so that dialog_util symbols are always available on Apple.
set(APPLE_DIALOG_MM_SOURCES "")
if (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs" AND NOT USE_METAL)
    set(APPLE_DIALOG_MM_SOURCES ${CORE_OPENGLES_SRC}/dialog-util-macos.mm)
    set_source_files_properties(${APPLE_DIALOG_MM_SOURCES}
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

# For Apple builds that use AVFoundation audio but NOT Metal, the
# audio-avfoundation.mm is not picked up by the Metal .mm glob above.
set(APPLE_AVF_AUDIO_MM_SOURCES "")
if (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs" AND ${AUDIO} STREQUAL "avfoundation" AND NOT USE_METAL)
    set(APPLE_AVF_AUDIO_MM_SOURCES ${CORE_OPENGLES_SRC}/audio-avfoundation.mm)
    set_source_files_properties(${APPLE_AVF_AUDIO_MM_SOURCES}
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

#Generate the library from the sources (STATIC on iOS — App Store forbids bare dylibs)
if(PLAT_LOWER STREQUAL "ios")
    set(_CORE_MBM_LIB_TYPE STATIC)
else()
    set(_CORE_MBM_LIB_TYPE SHARED)
endif()
add_library(core_mbm ${_CORE_MBM_LIB_TYPE}
    ${STB_SOURCES} 
    ${LODE_PNG_SOURCES} 
    ${MINI_Z_SOURCES} 
    ${SOURCES}
    ${RENDER_SOURCES}
    ${TINYFILEDIALOGS_SOURCES}
    ${PORT_AUDIO_SOURCES}
    ${WAVE_SOURCES}
    ${METAL_MM_SOURCES}
    ${METAL_RENDER_MM_SOURCES}
    ${APPLE_DIALOG_MM_SOURCES}
    ${APPLE_AVF_AUDIO_MM_SOURCES}
    ${PLUS_WINDOWS_SOURCES})

# Static libraries on iOS do not need Info.plist.  Set these attributes
# directly on the target (the global CMAKE_XCODE_ATTRIBUTE_* doesn't reach
# sub-project .xcodeproj files, causing a red file reference in the navigator).
if(PLAT_LOWER STREQUAL "ios" AND CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(core_mbm PROPERTIES
        XCODE_ATTRIBUTE_INFOPLIST_FILE ""
        XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE NO
        XCODE_ATTRIBUTE_OTHER_LIBTOOLFLAGS "-no_warning_for_no_symbols")
endif()

if(MBM_ENABLE_MESH_LEGACY_V7)
    target_compile_definitions(core_mbm PRIVATE MBM_ENABLE_MESH_LEGACY_V7)
endif()

# Define CORE_EXPORTS so API_IMPL resolves to dllexport (not dllimport) on Windows
target_compile_definitions(core_mbm PRIVATE CORE_EXPORTS)

    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    target_link_libraries(core_mbm ${OPENGLES2_LIBRARY})

    if (${AUDIO} STREQUAL "portaudio" )
        target_link_libraries(core_mbm ${PORT_AUDIO_LIBRARY})
    elseif(${AUDIO} STREQUAL "audiere" )
        target_link_libraries(core_mbm ${AUDIERE_LIBRARY})
    elseif(${AUDIO} STREQUAL "opensl" )
        add_definitions(-DAUDIO_ENGINE_ANDROID_OPENSL)
        target_link_libraries(core_mbm OpenSLES)
        message("Using OpenSL ES audio for ANDROID ")
    elseif(${AUDIO} STREQUAL "none")
        add_definitions(-DAUDIO_ENGINE_NONE)
        message("Disabling sound audio class for ANDROID ")
    else()
        message(SEND_ERROR " Unknow AUDIO choice: ${AUDIO}")
    endif()

    find_library(ANDROID_LOG_LIB log)
    target_link_libraries(core_mbm log android EGL GLESv2)

    
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

    target_link_libraries(core_mbm ${X11_LIBRARIES})
    target_link_libraries(core_mbm ${OPENGLES2_LIBRARY})
    target_link_libraries(core_mbm ${EGL_LIBRARY})
    if (${AUDIO} STREQUAL "portaudio" )
        target_link_libraries(core_mbm ${PORT_AUDIO_LIBRARY})
    elseif(${AUDIO} STREQUAL "audiere" )
        # Audiere for Linux (trick)
        # Since it has it own MakeFile (not converted to cmake yet), we copy the generated library and added as IMPORTED library
        add_library(${AUDIERE_LIBRARY} SHARED IMPORTED)
        set_target_properties(${AUDIERE_LIBRARY} PROPERTIES IMPORTED_LOCATION "${FOLDER_BIN}/${FOLDER_ARCHITECTURE}/libaudiere-1.10.1.so")
        add_dependencies(core_mbm audiere_cp)
        target_link_libraries(core_mbm ${AUDIERE_LIBRARY})
        # End Audiere for Linux
    elseif(${AUDIO} STREQUAL "none")
        message("Disabling sound audio class for LINUX ")
    else()
        message(SEND_ERROR " Unknow AUDIO choice: ${AUDIO}")
    endif()
    
    target_link_libraries(core_mbm dl)
    target_link_libraries(core_mbm pthread)
    
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")

    # ── Audio ────────────────────────────────────────────────────────────────
    if(AUDIO STREQUAL "audiere")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_link_libraries(core_mbm
                "${CMAKE_SOURCE_DIR}/third-party/audiere-1.9.4/bin/x64/audiere.lib")
        else()
            target_link_libraries(core_mbm
                "${CMAKE_SOURCE_DIR}/third-party/audiere-1.9.4/lib/audiere.lib")
        endif()
    elseif(AUDIO STREQUAL "none")
        message(STATUS "Disabling audio for Windows")
    else()
        message(SEND_ERROR "Unsupported AUDIO backend for Windows MinGW: ${AUDIO}. Use -DAUDIO=audiere or -DAUDIO=none")
    endif()

    # ── Graphics backend ─────────────────────────────────────────────────────
    if(USE_DIRECTX9)
        target_link_libraries(core_mbm d3d9 d3dcompiler)
    else()
        # OpenGL ES emulation (Imagination PowerVR pre-built import libs)
        target_link_libraries(core_mbm
            "${CMAKE_SOURCE_DIR}/third-party/gles/libs/libEGL.dll.lib"
            "${CMAKE_SOURCE_DIR}/third-party/gles/libs/libGLESv2.dll.lib")
    endif()

    # ── Windows system libraries ──────────────────────────────────────────────
    # msimg32: TransparentBlt (plusWindows)
    # comctl32: InitCommonControlsEx, CreateUpDownControl (plusWindows)
    # hid:     HidP_* functions (joystick-win32)
    target_link_libraries(core_mbm winmm gdi32 ws2_32 ole32 msimg32 comctl32 hid)
    target_link_libraries(core_mbm Threads::Threads)

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    if(USE_METAL)
        target_link_libraries(core_mbm "-framework Metal")
        target_link_libraries(core_mbm "-framework Foundation")
        target_link_libraries(core_mbm "-framework QuartzCore")
        target_link_libraries(core_mbm "-framework Cocoa")
        target_link_libraries(core_mbm "-framework AppKit")
    else()
        # Non-Metal Apple builds (e.g. USE_DUMMY_BACK_END_ENGINE) still need Cocoa
        # for dialog-util-macos.mm (NSOpenPanel / NSSavePanel).
        target_link_libraries(core_mbm "-framework Cocoa")
        target_link_libraries(core_mbm "-framework Foundation")
        target_link_libraries(core_mbm ${X11_LIBRARIES})
        target_link_libraries(core_mbm ${OPENGLES2_LIBRARY})
        target_link_libraries(core_mbm ${EGL_LIBRARY})
        target_link_libraries(core_mbm ${VULKAN_LIBRARY})
    endif()
    if (${AUDIO} STREQUAL "portaudio" )
        target_link_libraries(core_mbm ${PORT_AUDIO_LIBRARY})
    elseif(${AUDIO} STREQUAL "avfoundation")
        target_link_libraries(core_mbm "-framework AVFoundation")
        target_link_libraries(core_mbm "-framework AudioToolbox")
    elseif(${AUDIO} STREQUAL "audiere" )
        # Audiere for Linux (trick)
        # Since it has it own MakeFile (not converted to cmake yet), we copy the generated library and added as IMPORTED library
        add_library(${AUDIERE_LIBRARY} SHARED IMPORTED)
        set_target_properties(${AUDIERE_LIBRARY} PROPERTIES IMPORTED_LOCATION "${FOLDER_BIN}/${FOLDER_ARCHITECTURE}/libaudiere-1.10.1.so")
        add_dependencies(core_mbm audiere_cp)
        target_link_libraries(core_mbm ${AUDIERE_LIBRARY})
        # End Audiere for Linux
    elseif(${AUDIO} STREQUAL "none")
        message("Disabling sound audio class for MACOS ")
    else()
        message(SEND_ERROR " Unknow AUDIO choice: ${AUDIO}")
    endif()
    
    target_link_libraries(core_mbm dl)
    target_link_libraries(core_mbm pthread)

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    # iOS uses Metal + UIKit; no Cocoa/AppKit
    target_link_libraries(core_mbm "-framework Metal")
    target_link_libraries(core_mbm "-framework Foundation")
    target_link_libraries(core_mbm "-framework QuartzCore")
    target_link_libraries(core_mbm "-framework UIKit")
    target_link_libraries(core_mbm "-framework CoreGraphics")
    if (${AUDIO} STREQUAL "avfoundation")
        target_link_libraries(core_mbm "-framework AVFoundation")
        target_link_libraries(core_mbm "-framework AudioToolbox")
    elseif(${AUDIO} STREQUAL "none")
        message("Disabling sound audio class for iOS")
    else()
        message(SEND_ERROR " Unknown AUDIO choice for iOS: ${AUDIO}")
    endif()
    target_link_libraries(core_mbm pthread)

else()
    message( SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM=${ENGINE_TARGET_PLATFORM} in src/CMakeLists.txt")
endif()