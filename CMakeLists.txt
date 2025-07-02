#https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html

#selecting compiler
#sudo update-alternatives --set cc /usr/bin/gcc
#sudo update-alternatives --set c++ /usr/bin/g++
#
#sudo update-alternatives --set cc /usr/bin/clang
#sudo update-alternatives --set c++ /usr/bin/clang++

#leak (using Gcc)
#add_compile_options(-fsanitize=address,undefined)
#add_compile_options(-fno-omit-frame-pointer)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")


cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)

set(CMAKE_SUCCESS_SET_PLAT           0)
set(OPTIMIZED                    "-O3")

STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

if (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    set(FOLDER_BIN "${CMAKE_CURRENT_LIST_DIR}/bin/debug")
    set(FOLDER_LIB "${CMAKE_CURRENT_LIST_DIR}/libs/debug")
else()
    set(FOLDER_BIN "${CMAKE_CURRENT_LIST_DIR}/bin/release")
    set(FOLDER_LIB "${CMAKE_CURRENT_LIST_DIR}/libs/release")
endif()

set(ROOT_FOLDER "${CMAKE_CURRENT_LIST_DIR}")


file(MAKE_DIRECTORY ${FOLDER_BIN})
file(MAKE_DIRECTORY ${FOLDER_LIB})

message("\n")
message("+-------------------------------------------------------------+")
message("|For editor (Visual studio code, Sublime, etc...)             |")
message("|Set the environment variable PLAT_EDITOR to specific platform|")
message("+-------------------------------------------------------------+")
message("\n")

function(SHOW_OPTIONS_SELECTED)
    if(USE_LUA)
        message("-DUSE_LUA=1                : Provide LUA engine embedded on core_mbm")
    else()
        message("-DUSE_LUA=0                : Provide LUA engine embedded on core_mbm")
    endif()
    if(USE_EDITOR_FEATURES)
        message("-DUSE_EDITOR_FEATURES=1    : FONT_DRAW, Load from memory meshDebug, MESH_MBM_DEBUG  embedded on core_mbm")
    else()
        message("-DUSE_EDITOR_FEATURES=0    : FONT_DRAW, Load from memory meshDebug, MESH_MBM_DEBUG  embedded on core_mbm")
    endif()
    if(USE_VR)
        message("-DUSE_VR=1                 : Provide VR class embedded on LUA engine (Default 1 (Linux, Windows), Default 0 (Android), use -DDISABLE_VR=1 to disable)")
    else()
        message("-DUSE_VR=0                 : Provide VR class embedded on LUA engine (Default 1 (Linux, Windows), Default 0 (Android), use -DDISABLE_VR=1 to disable)")
    endif()

    message("-DAUDIO=${AUDIO}                   : Select the audio engine available. Default: Linux -DAUDIO=portaudio , Windows -DAUDIO=audiere , Android -DAUDIO=jni , -DAUDIO=none will remove audio dependency")

    if(USE_ALL)
        message("-DUSE_ALL=1                : Enable all features available")
    else()
        message("-DUSE_ALL=0                : DO Not Enable all features available")
    endif()
    if(PLAT_LOWER STREQUAL "android")
        if(USE_STL_STATIC)
            message("-DUSE_STL_STATIC=1         : Use stl static (c++_static) lib. '0' for c++_shared")
        else()
            message("-DUSE_STL_STATIC=0         : Use stl shared (c++_shared) lib. '1' for  c++_static")
        endif()
        message("-DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL}")
    endif()
    message("******************************************************************************************************")
endfunction()

function(SHOW_OTHERS_OPTIONS_ENGINE)
    message("Options/Features used to the engine:")
    
    if(USE_OPENGL_ES)
        message("-DUSE_OPENGL_ES=1          : Use opengl as backend ENABLED")
    else()
        message("-DUSE_OPENGL_ES=0          : Use opengl as backend DISABLED")
    endif()
    if(USE_DEPRECATED_2_MINOR)
        message("-DUSE_DEPRECATED_2_MINOR=1 : Compatibility with old binary (version 1) ENABLED ")
    else()
        message("-DUSE_DEPRECATED_2_MINOR=0 : Compatibility with old binary (version 1) DISABLED")
    endif()
    if(DEBUG_SET_GET_GLOBAL)
        message("-DDEBUG_SET_GET_GLOBAL=1   : Debug for get and set global instruction using LUA ENABLED")
    else()
        message("-DDEBUG_SET_GET_GLOBAL=0   : Debug for get and set global instruction using LUA DISABLED")
    endif()
    message("******************************************************************************************************")
endfunction()

function(SHOW_ANDROID_OPTIONS)
    message("Options for Android:")
    message("-DANDROID_ABI=arm64-v8a                : (principal 64) ARMv8 AArch64 instruction set - since NDK r10")
    message("-DANDROID_ABI=armeabi-v7a              : (principal)    ARMv7 based devices with hardware FPU instructions (VFPv3_D16);")
    message("-DANDROID_ABI='armeabi-v7a with NEON'  : same as armeabi-v7a, but sets NEON as floating-point unit;")
    message("-DANDROID_ABI='armeabi-v7a with VFPV3' : same as armeabi-v7a, but sets VFPv3_D32 as floating-point unit;")
    message("-DANDROID_ABI=x86                      : IA-32 instruction set")
    message("-DANDROID_ABI=mips                     : MIPS32 instruction set")
    message("-DANDROID_ABI=mips64                   : MIPS64 instruction set (r6) - since NDK r10")
    message("-DANDROID_ABI=x86_64                   : Intel64 instruction set (r1) - since NDK r10")
    message("-DANDROID_NATIVE_API_LEVEL=24          : MINSDKVERSION Android 8.0 (API level 24) (Nougat)")
    if(USE_STL_STATIC)
        message("-DUSE_STL_STATIC=1                     : Use stl static (c++_static) lib. '0' for c++_shared")
    else()
        message("-DUSE_STL_STATIC=0                     : Use stl shared (c++_shared) lib. '1' for  c++_static (Not recomened)")
    endif()
    if(USE_LSQLITE3)
        message("-DUSE_LSQLITE3=1                       : lSqLite3 is enabled")
    else()
        message("-DUSE_LSQLITE3=0                       : use -DUSE_LSQLITE3=1 to build lsqlite3")
    endif()
    if(USE_BOX2D)
        message("-DUSE_BOX2D=1                          : Box2d is enabled")
    else()
        message("-DUSE_BOX2D=0                          : use -DUSE_BOX2D=1 to build box2d")
    endif()
    if(USE_IMGUI)
        message("-DUSE_IMGUI=1                          : ImGui is enabled")
    else()
        message("-DUSE_IMGUI=0                          : use -DUSE_IMGUI=1 to build ImGui")
    endif()
endfunction()

if(NOT PLAT)
    message("Platform not set...")
    if(DEFINED ENV{PLAT_EDITOR})
        set(PLAT $ENV{PLAT_EDITOR})
        set(USE_ALL True)
        message("Platform editor set as ${PLAT}")
    endif()
endif()

if(NOT PLAT)
    message("******************************************************************************************************")
    message("Platform NOT defined!")
    message("Please type:")
    message("cmake -DPLAT= <Windows>, <Linux>, <Android> or <Apple>" )
    SHOW_ANDROID_OPTIONS()
    message("Please remove the folder 'CMakeFiles' and the file 'CMakeCache.txt' before try to run 'cmake' again!")
    message("")
    message("******************************************************************************************************")
    message("Options/features available to the engine:")
    SHOW_OPTIONS_SELECTED()

    message(SEND_ERROR "Aborting...")
else()
    STRING(TOLOWER "${PLAT}" PLAT_LOWER)

    if(PLAT_LOWER STREQUAL "android")

        message("NDK_ROOT:"         $ENV{NDK_ROOT})

        #Android version
        set(CMAKE_SYSTEM_NAME           Android)
        set(ENGINE_TARGET_PLATFORM      "Android")
        if(NOT ANDROID_NATIVE_API_LEVEL)
            message(STATUS "ANDROID_NATIVE_API_LEVEL set to default:24 ")
            set(ANDROID_NATIVE_API_LEVEL    24)#  $MINSDKVERSION Android 8.0 (API level 24) (Nougat)
        endif()
        set(ANDROID_PLATFORM            "android-${ANDROID_NATIVE_API_LEVEL}")# MINSDKVERSION
        set(CMAKE_SYSTEM_VERSION        ${ANDROID_NATIVE_API_LEVEL}) # API level
        set(CMAKE_ANDROID_API           ${ANDROID_NATIVE_API_LEVEL}) # API level
        set(NDK_ROOT                    $ENV{NDK_ROOT})
        set(CMAKE_TOOLCHAIN_FILE        ${NDK_ROOT}/build/cmake/android.toolchain.cmake)
        set(CMAKE_ANDROID_NDK           ${NDK_ROOT})

        if(USE_STL_STATIC)
            set(CMAKE_ANDROID_STL_TYPE      c++_static)#c++_shared (CMAKE_ANDROID_STL_TYPE does not work!!!)
            set(ANDROID_STL                 c++_static)#c++_shared
        else()
            set(CMAKE_ANDROID_STL_TYPE      c++_shared)#gnustl_static (CMAKE_ANDROID_STL_TYPE does not work!!!)
            set(ANDROID_STL                 c++_shared)#gnustl_static
        endif()
        
        set(APP_OPTIM                   release)
        set(ANDROID_LD                  on)
        set(ANDROID_TOOLCHAIN           clang)#gcc does not work anymore
        
        
        if(${ANDROID_NATIVE_API_LEVEL} GREATER_EQUAL 23) #True is Default for API level 23 or newer.
            message(STATUS "ARM_NEON Enabled...")
            set(CMAKE_ANDROID_ARM_NEON      TRUE)#set to armeabi-v7a.  set CMAKE_ANDROID_ARM_NEON to ON to target ARM NEON devices.
            set(ANDROID_ARM_NEON            TRUE)
        endif()

        add_definitions(-DANDROID)

        if(NOT USE_VR AND NOT DISABLE_VR)
            set(DISABLE_VR 1)
        endif()

        
        if(NOT ANDROID_ABI)
            message("******************************************************************************************************")
            message("ANDROID_ABI is not set...")
            SHOW_ANDROID_OPTIONS()
            message("Setting to arm64-v8a")
            message("")
            set(CMAKE_ANDROID_ARCH_ABI      "arm64-v8a")
            set(ANDROID_ABI                 "arm64-v8a")
        else()
            if(ANDROID_ABI STREQUAL "armeabi-v7a")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to armeabi-v7a")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "armeabi-v7a")
            elseif(ANDROID_ABI STREQUAL "armeabi-v7a with NEON")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to armeabi-v7a with NEON")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "armeabi-v7a with NEON")
                set(CMAKE_ANDROID_ARM_NEON      TRUE)#is set to armeabi-v7a set CMAKE_ANDROID_ARM_NEON to ON to target ARM NEON devices.
                set(ANDROID_ARM_NEON            TRUE)
            elseif(ANDROID_ABI STREQUAL "armeabi-v7a with VFPV3")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to armeabi-v7a with VFPV3 however I am not sure if it is working :/")
                message("There is a problem: https://github.com/android-ndk/ndk/issues/906")
                message("We here are setting ANDROID_ARM_MODE to  thumb otherwise will not compile using android-ndk-r20 (current this test)")
                message("******************************************************************************************************")
                set(ANDROID_ABI                 "armeabi-v7a")
                set(CMAKE_ANDROID_ARCH_ABI      "armeabi-v7a with VFPV3")
                set(CMAKE_ANDROID_ARM_MODE      thumb)#ON to target 32-bit ARM processors, default is to target the 16-bit Thumb processors
                set(ANDROID_ARM_MODE            thumb)#tumb or arm
            elseif(ANDROID_ABI STREQUAL "x86")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to x86")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "x86")
            elseif(ANDROID_ABI STREQUAL "mips")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to mips")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "mips")
            elseif(ANDROID_ABI STREQUAL "mips64")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to mips64")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "mips64")
            elseif(ANDROID_ABI STREQUAL "arm64-v8a")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to arm64-v8a")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "arm64-v8a")
            elseif(ANDROID_ABI STREQUAL "x86_64")
                message("******************************************************************************************************")
                message("ANDROID_ABI is set to x86_64")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "x86_64")
            else()
                message("******************************************************************************************************")
                message("ANDROID_ABI is unknown: [${ANDROID_ABI}]")
                message("Setting to armeabi-v7a")
                message("******************************************************************************************************")
                set(CMAKE_ANDROID_ARCH_ABI      "armeabi-v7a")
                set(ANDROID_ABI                 "armeabi-v7a")
            endif()
        endif()
        
        set(ABI                     ${ANDROID_ABI})
        set(FOLDER_ARCHITECTURE     ${CMAKE_ANDROID_ARCH_ABI})

        if(NOT AUDIO)
            set(AUDIO jni)
        endif()

        if(NOT DEFINED ENV{NDK_ROOT})

            message(SEND_ERROR "NDK_ROOT is unset ... aborting" )

        elseif(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})

            message(SEND_ERROR "toolchain DO NOT found at ${CMAKE_TOOLCHAIN_FILE}" )
        
        else()

            #success
            set(CMAKE_SUCCESS_SET_PLAT           1)

        endif()
    elseif(PLAT_LOWER STREQUAL "windows")

        if(NOT USE_VR AND NOT DISABLE_VR)
            set(USE_VR 1)
            add_definitions(-DUSE_VR)
        endif()

        if(NOT AUDIO)
            set(AUDIO audiere)
        endif()

        set(CMAKE_GENERATOR_PLATFORM    "Visual Studio 15 2017 ARM")
        set(DWIN32 1)
        set(D_WINDOWS 1)
        set(WIN32 1)
        set(_WINDOWS 1)
        set(Platform     "Win32")
        add_definitions(-DWIN32)
        add_definitions(-D_WINDOWS)
        set(CMAKE_SYSTEM_NAME           "Windows")
        set(ENGINE_TARGET_PLATFORM      "Windows")
        set(FOLDER_ARCHITECTURE         "win32")

        set(CMAKE_SUCCESS_SET_PLAT           1)
        message(SEND_ERROR " Platform NOT working yet: ${PLAT}")

    elseif(PLAT_LOWER STREQUAL "linux")

        set(CMAKE_SYSTEM_NAME           "Linux")
        set(ENGINE_TARGET_PLATFORM      "Linux")
        set(FOLDER_ARCHITECTURE         "linux_x86")
        #success
        set(CMAKE_SUCCESS_SET_PLAT           1)

        if(NOT USE_VR AND NOT DISABLE_VR)
            set(USE_VR 1)
            add_definitions(-DUSE_VR)
        endif()

        if(NOT AUDIO)
            set(AUDIO portaudio)
        endif()
        
    elseif(PLAT_LOWER STREQUAL "apple")

        message(SEND_ERROR " Platform NOT working yet: ${PLAT}")

    else()

        message(SEND_ERROR " Platform unknown ${PLAT}")
        
    endif()
endif()


if(FOLDER_ARCHITECTURE)
    #specific architecture
    file(MAKE_DIRECTORY ${FOLDER_BIN}/${FOLDER_ARCHITECTURE})
    file(MAKE_DIRECTORY ${FOLDER_LIB}/${FOLDER_ARCHITECTURE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${FOLDER_LIB}/${FOLDER_ARCHITECTURE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${FOLDER_BIN}/${FOLDER_ARCHITECTURE})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${FOLDER_BIN}/${FOLDER_ARCHITECTURE})
endif()

if(CMAKE_SUCCESS_SET_PLAT EQUAL 1)

    #Here starts to check project/working C/C++ compiler
    project(mini-mbm)
    
    #set(CMAKE_VERBOSE_MAKEFILE on)

    set(LUA_LIB_VERSION         "5.4.1")
    set(BOX2D_LIB_VERSION       "2.3.1")
    set(BULLET_LIB_VERSION      "2.84")
    

    set(VERSION_MAJOR 1)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)

    set(SRC 				"src")
    set(MODULES     		"modules")
    set(THIRD_PARTY 		"third-party")
    set(PLUGINS     		"plugins")
    set(LUA_LIB 			${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
    set(SQLITE_LIB 			${THIRD_PARTY}/lsqlite3)
    set(BULLET_LIB 			${PLUGINS}/bullet3d)
    set(IMGUI_LIB           ${PLUGINS}/imGui)
    set(BOX2D_LIB   		${PLUGINS}/box2d)
    set(TILED_LIB   		${PLUGINS}/tiled)
    set(AUDIERE             ${THIRD_PARTY}/audiere-1.9.4)
    set(PORT_AUDIO_LIB 		${THIRD_PARTY}/portaudio)
    
    
    #libraries name
    set(LUA_LIBRARY                 lua-${LUA_LIB_VERSION})
    set(CORE_MBM_LIBRARY            core_mbm)
    set(SQLITE3_LUA_LIBRARY         lsqlite3)
    set(BULLET_3D_LIBRARY           bullet3d)
    set(BOX2D_LIBRARY               box2d)
    set(IMGUI_LIBRARY               ImGui)
    set(TILED_LIBRARY               tilemap)
    set(AUDIERE_LIBRARY             audiere)
    set(PORT_AUDIO_WRAPPER_LIBRARY  pa_wrapper)
    
    
    set(CORE_MBM_LIB 			${SRC}/core_mbm)
    set(TEST_LIB 			    ${SRC}/test-lib)
    set(MINI_MBM                ${SRC})

    message(STATUS " ENGINE_TARGET_PLATFORM ${ENGINE_TARGET_PLATFORM}")

    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS " CMAKE_BUILD_TYPE Not Set" )
        message(STATUS " CMAKE_BUILD_TYPE setting it to Release..." )
        set(CMAKE_BUILD_TYPE Release)
    else()
        message(STATUS " CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}" )
    endif()

    if (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
        set(CMAKE_BUILD_TYPE Debug)
    else()
        set(CMAKE_BUILD_TYPE Release)
    endif()

    if(USE_ALL)
        message(STATUS "Enabling all features available")
        set(USE_VR                  1)
        set(USE_EDITOR_FEATURES     1)
        set(USE_LUA                 1)
        if(PLAT_LOWER STREQUAL "android")
            set(USE_BOX2D               1)
            set(USE_IMGUI               1)
            set(USE_LSQLITE3            1)
        endif()
    endif()

    OPTION(DEBUG_SET_GET_GLOBAL "debug getGlobal and setglobal lua" OFF) # Disabled by default
    IF(DEBUG_SET_GET_GLOBAL)
        add_definitions(-DDEBUG_SET_GET_GLOBAL)
    ENDIF(DEBUG_SET_GET_GLOBAL)

    if(NOT PLAT_LOWER STREQUAL "android")
        if (USE_DEPRECATED_2_MINOR)
            if(USE_DEPRECATED_2_MINOR EQUAL 1)
                set(USE_DEPRECATED_2_MINOR      1)
                add_definitions(-DUSE_DEPRECATED_2_MINOR)
            endif()
        else()
            set(USE_DEPRECATED_2_MINOR      1)
            add_definitions(-DUSE_DEPRECATED_2_MINOR)
        endif()
    elseif (USE_DEPRECATED_2_MINOR)
        if(USE_DEPRECATED_2_MINOR EQUAL 1)
            set(USE_DEPRECATED_2_MINOR      1)
            add_definitions(-DUSE_DEPRECATED_2_MINOR)
        endif()
    endif()

    set(USE_OPENGL_ES    1)
    #default definitions: opengl, deprecated version, VR, lua options default compatibility
    if(NOT USE_VR AND DISABLE_VR)
        message(STATUS " Disabling VR class")
        add_definitions(-DUSE_OPENGL_ES -DLUA_COMPAT_ALL -DLUA_ANSI)
    else()
        message(STATUS " VR class available")
        set(USE_VR 1)
        add_definitions(-DUSE_OPENGL_ES -DUSE_VR -DLUA_COMPAT_ALL -DLUA_ANSI)
    endif()

    if(USE_EDITOR_FEATURES)
        add_definitions(-DUSE_EDITOR_FEATURES)
    endif()

    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
        message(STATUS " WHAT Configuration for Windows is set...")
        set(LIB_EGL_HELD "third-party/gles/libs/libEGL")
        set(LIB_GLESv2_HELD "third-party/gles/libs/libGLESv2")
        set(LIB_audiere_HELD "third-party/audiere-1.9.4/lib/audiere")
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        message(STATUS " Configuration for Android is set...")
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        message(STATUS " Configuration for Linux (Debian, ubuntu,...) is set...")
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")
        message(STATUS " Configuration for Apple (not working yet!!!) is set...")
    endif()


    #-fPIC : The -fPIC option tells the compiler to generate position independent code (PIC).
    # This is needed because the code in a shared library may be loaded into a different memory location
    # for different executables.
    # So, it's important to generate PIC code for shared libraries so that user code deos not dependent
    # on the absolute memory address of symbols.

    if (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
        
        set(_G_DEBUG " -g")
        set(OPTIMIZED "-O0")
        add_definitions(-D_DEBUG)

        if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
            set(NDK_DEBUG 1)
            add_definitions(-DNDK_DEBUG)
        endif()

    endif()

    
    if (USE_LUA)

        message(STATUS "Using LUA script...")
        if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
                set(CMAKE_CXX_FLAGS " -fPIC ${OPTIMIZED} -Wall ${_G_DEBUG} -std=c++17")
                set(CMAKE_C_FLAGS "${OPTIMIZED} -Wall")
            elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
                set(CMAKE_CXX_FLAGS " -fPIC ${OPTIMIZED} -Wall ${_G_DEBUG} -static-libstdc++ -std=c++17")
                set(CMAKE_C_FLAGS "${OPTIMIZED} -Wall")
            elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
                set(CMAKE_CXX_FLAGS " -fPIC ${OPTIMIZED} -Wall ${_G_DEBUG} -static-libstdc++ -std=c++17")
                set(CMAKE_C_FLAGS "${OPTIMIZED} -Wall")
            endif()
        elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
                set(CMAKE_CXX_FLAGS " -W4 -O3 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS")
            endif()
        elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
            set(CMAKE_CXX_FLAGS " -fPIC -O3 -Wall ${_G_DEBUG} -std=c++17")
            set(CMAKE_C_FLAGS "-O3 -Wall")
        endif()

    else()

        message(INFO " Using pure CPP...")
        if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
                set(CMAKE_CXX_FLAGS " -fPIC ${OPTIMIZED} -Wall ${_G_DEBUG} -std=c++17")
                set(CMAKE_C_FLAGS "${OPTIMIZED} -Wall")
            elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
                set(CMAKE_CXX_FLAGS " -fPIC ${OPTIMIZED} -Wall ${_G_DEBUG} -static-libstdc++ -std=c++17")
                set(CMAKE_C_FLAGS "${OPTIMIZED} -Wall")
            elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
                set(CMAKE_CXX_FLAGS " -fPIC ${OPTIMIZED} -Wall ${_G_DEBUG} -static-libstdc++ -std=c++17")
                set(CMAKE_C_FLAGS "${OPTIMIZED} -Wall")
            endif()
        elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
                set(CMAKE_CXX_FLAGS " -W4 -O3 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS")
            endif()
        elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
            set(CMAKE_CXX_FLAGS " -fPIC -O3 -Wall ${_G_DEBUG} -std=c++17")
            set(CMAKE_C_FLAGS "-O3 -Wall")
        endif()

    endif()

    if (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
        message(STATUS " *** DEBUG mode ***")
    else()
        message(STATUS "*** RELEASE mode ***")
    endif()

    #plugins ################################################################################################
    # Plugins only make sense when use LUA
    if(USE_LUA)
        if(PLAT_LOWER STREQUAL "android")
            if(USE_IMGUI)
                add_subdirectory(${IMGUI_LIB} 		${IMGUI_LIB})
            endif()
        else()
            add_subdirectory(${IMGUI_LIB} 			${IMGUI_LIB})
        endif()

    #to be able to enable TILED_LIB it is needed enable USE_EDITOR_FEATURES
    #The following plugins are supposed to be 'helpers' in editor. We are not going to buid it for Android
        if(NOT PLAT_LOWER STREQUAL "android")
            if(USE_EDITOR_FEATURES)
                add_subdirectory(${TILED_LIB}       ${TILED_LIB})
            endif()
        endif()

    
        if(PLAT_LOWER STREQUAL "android")
            if(USE_BOX2D)
                add_subdirectory(${BOX2D_LIB}        ${BOX2D_LIB})
            endif()
        else()
            add_subdirectory(${BOX2D_LIB}            ${BOX2D_LIB})
        endif()
    
        if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
            add_subdirectory(${BULLET_LIB} 			${BULLET_LIB})
        endif()

        if(PLAT_LOWER STREQUAL "android")
            if(USE_LSQLITE3)
                add_subdirectory(${SQLITE_LIB} 		${SQLITE_LIB})
            endif()
        else()
            add_subdirectory(${SQLITE_LIB} 			${SQLITE_LIB})
        endif()

        if(NOT PLAT_LOWER STREQUAL "android")
            add_subdirectory(${MODULES}                     ${MODULES})
        endif()
    endif()
    #end plugins ############################################################################################
        
    if (USE_LUA)
        add_definitions(-DUSE_LUA)
        add_subdirectory(${LUA_LIB} 			${LUA_LIB})
    endif()
    
    add_subdirectory(${CORE_MBM_LIB} 		        ${CORE_MBM_LIB})

    # in case you want a wrapper over portaudio uncomment this
    if (USE_PA_WRAPPER AND ${AUDIO} STREQUAL "portaudio" )
        add_subdirectory(${PORT_AUDIO_LIB} 			${PORT_AUDIO_LIB})
    endif()

    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        add_subdirectory(${TEST_LIB} 				${TEST_LIB})
    endif()

    if(AUDIO STREQUAL "audiere")
        add_subdirectory(${AUDIERE}                     ${AUDIERE})
    endif()
    
    add_subdirectory(${MINI_MBM}                    ${MINI_MBM})

    find_program(LS ls
            HINTS /bin)
    if (LS)
        add_custom_target(  ls ALL
                            COMMAND ${LS} -lSh "${FOLDER_BIN}/${FOLDER_ARCHITECTURE}/*"
                            DEPENDS ls mini-mbm ${SQLITE_LIB} ${BULLET_LIB} ${IMGUI_LIB} ${BOX2D_LIB})
    else()
        message("NOT found ls command!")
    endif()


    message("")
    message("******************************************************************************************************")
    message("Options/Features used in this compilation:")
    SHOW_OPTIONS_SELECTED()
    SHOW_OTHERS_OPTIONS_ENGINE()
else()

    message(SEND_ERROR " Failled to set platform: ${PLAT}")

endif()
