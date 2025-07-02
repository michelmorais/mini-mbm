cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(${IMGUI_LIBRARY})

set(SRC_INCLUDES            "../../include")
set(IMGUI_INCLUDE ".")
set(IMGUI_SRC ".")
set(THIRD_PARTY                 ${CMAKE_SOURCE_DIR}/third-party)
set(LUA_INCLUDE                 ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-char-subscripts")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

add_definitions(-DPLUGIN_CALLBACK)
add_definitions(-DIMGUI_IMPL_OPENGL_ES2)
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM)
message(STATUS "Building IMGUI lib...")
file(GLOB_RECURSE IM_GUI_SOURCES        "${IMGUI_SRC}/*.cpp")

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
    list(REMOVE_ITEM IM_GUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_SRC}/imgui_impl_win32.cpp")
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

    find_package(X11 REQUIRED)

    message(STATUS "X11_FOUND = ${X11_FOUND}")
    message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
    message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")

    include_directories(${X11_INCLUDE_DIR})
    link_directories(${X11_LIBRARIES})
    ####
  endif()

#Bring the headers, such as xxx.h into the project
include_directories(${IMGUI_INCLUDE} ${LUA_INCLUDE} ${SRC_INCLUDES} ${THIRD_PARTY})
 
# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

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
endif()
set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${IMGUI_LIBRARY} SHARED ${IM_GUI_SOURCES})

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
  target_link_libraries(${IMGUI_LIBRARY} ${OPENGLES2_LIBRARY})
endif()

target_link_libraries(${IMGUI_LIBRARY} ${LUA_LIBRARY})
target_link_libraries(${IMGUI_LIBRARY} ${CORE_MBM_LIBRARY})
