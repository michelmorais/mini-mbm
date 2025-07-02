cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(core_mbm)

set(SRC_INCLUDES            "../../include")
set(CORE_OPENGLES_SRC       ${CMAKE_CURRENT_SOURCE_DIR})
set(RENDER_SRC              "../render")
set(THIRD_PARTY             "../../third-party")
set(RENDER_INCLUDE          ${SRC_INCLUDES}/render)
set(CORE_OPENGLES_INCLUDE   ${SRC_INCLUDES}/core_mbm)
set(BOX2D_WRAP_INCLUDE      ${SRC_INCLUDES}/box-2d-wrap)
set(BOX2D_WRAP_SRC          box-2d-wrap)
set(BULLET3d_WRAP_INCLUDE   ${SRC_INCLUDES}/bullet-3d-wrap)
set(BULLET3d_WRAP_SRC       bullet-3d-wrap)
set(CORE_OPENGLES_MINIZ     ${CORE_OPENGLES_SRC}/miniz)
set(CORE_OPENGLES_PLATFORM  ${CORE_OPENGLES_SRC}/platform)
set(STB_DIR                 ${THIRD_PARTY}/stb)
set(MINIZ_DIR               ${THIRD_PARTY}/miniz)
set(LODE_PNG_DIR            ${THIRD_PARTY}/lodepng)
set(BOX2D_INC               ${THIRD_PARTY}/box2d-${BOX2D_LIB_VERSION})
set(BULLET_3D_INC           ${THIRD_PARTY}/bullet-${BULLET_LIB_VERSION})
set(TINYFILEDIALOGS_DIR     ${THIRD_PARTY}/tinyfiledialogs)
set(PORT_AUDIO_SRC          ${THIRD_PARTY}/portaudio/src)
set(PORT_AUDIO_INCLUDE      ${THIRD_PARTY}/portaudio/include)
set(WAVE_SRC                ${THIRD_PARTY}/wave)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-misleading-indentation")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-misleading-indentation")
endif()


#Bring the headers, such as xxx.h into the project
include_directories(${CORE_OPENGLES_INCLUDE} 
                    ${GLES_INCLUDE}
                    ${RENDER_INCLUDE} 
                    ${CORE_OPENGLES_MINIZ} 
                    ${CORE_OPENGLES_PLATFORM} 
                    ${SRC_INCLUDES} 
                    ${STB_DIR} 
                    ${THIRD_PARTY} 
                    ${BOX2D_WRAP_INCLUDE} 
                    ${BOX2D_INC}
                    ${BULLET_3D_INC}
                    ${PORT_AUDIO_INCLUDE}
                    ${WAVE_SRC})

 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES                   "${CORE_OPENGLES_SRC}/*.cpp")
file(GLOB_RECURSE RENDER_SOURCES            "${RENDER_SRC}/*.cpp")
file(GLOB_RECURSE MINI_Z_SOURCES            "${MINIZ_DIR}/*.c")
file(GLOB_RECURSE STB_SOURCES               "${STB_DIR}/*.c")
file(GLOB_RECURSE LODE_PNG_SOURCES          "${LODE_PNG_DIR}/*.cpp")


if (${AUDIO} STREQUAL "audiere")
    add_definitions(-DAUDIO_ENGINE_AUDIERE)
elseif (${AUDIO} STREQUAL "jni")
    add_definitions(-DAUDIO_ENGINE_ANDROID_JNI)
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
elseif( NOT ${AUDIO} STREQUAL "none")
    message(SEND_ERROR " Unknow AUDIO choice: ${AUDIO}")
endif()

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
	#remove common-jni ANDROID stuff file
    list(REMOVE_ITEM SOURCES "${CORE_OPENGLES_PLATFORM}/common-jni.cpp")
    if(USE_EDITOR_FEATURES)
        file(GLOB_RECURSE TINYFILEDIALOGS_SOURCES   "${TINYFILEDIALOGS_DIR}/*.c")
    endif()    
endif()


if (${AUDIO} STREQUAL "audiere")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-none.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-portaudio.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-jni-android.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-direct-sound.cpp")
elseif(${AUDIO} STREQUAL "jni")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-none.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-portaudio.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-audiere.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-direct-sound.cpp")
elseif(${AUDIO} STREQUAL "portaudio")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-none.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-jni-android.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-audiere.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-direct-sound.cpp")
else(${AUDIO} STREQUAL "none")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-portaudio.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-jni-android.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-audiere.cpp")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/audio-direct-sound.cpp")
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

    message( SEND_ERROR "Not configurated for Windows src/CMakeList.txt")

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")

    message( SEND_ERROR "Not configurated for Apple src/CMakeList.txt")

endif()


#Generate the static library from the sources
add_library(core_mbm SHARED
    ${STB_SOURCES} 
    ${LODE_PNG_SOURCES} 
    ${MINI_Z_SOURCES} 
    ${SOURCES}
    ${RENDER_SOURCES}
    ${TINYFILEDIALOGS_SOURCES}
    ${PORT_AUDIO_SOURCES}
    ${WAVE_SOURCES})

    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    target_link_libraries(core_mbm ${OPENGLES2_LIBRARY})

    if(USE_EDITOR_FEATURES)
        message("{EGL_LIBRARY} ${EGL_LIBRARY}")
        target_link_libraries(core_mbm ${EGL_LIBRARY})
    endif()

    if (${AUDIO} STREQUAL "portaudio" )
        target_link_libraries(core_mbm ${PORT_AUDIO_LIBRARY})
    elseif(${AUDIO} STREQUAL "audiere" )
        target_link_libraries(core_mbm ${AUDIERE_LIBRARY})
    elseif(${AUDIO} STREQUAL "jni" )
        message("Using native audo JNI ANDROID ")
    elseif(${AUDIO} STREQUAL "none")
        message("Disabling sound audio class for ANDROID ")
    else()
        message(SEND_ERROR " Unknow AUDIO choice: ${AUDIO}")
    endif()

    find_library(ANDROID_LOG_LIB log)
    target_link_libraries(core_mbm log android)#android log

    
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

    message(" ... ")
    
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")
    message( SEND_ERROR "Not configurated for Apple src/CMakeList.txt")
endif()