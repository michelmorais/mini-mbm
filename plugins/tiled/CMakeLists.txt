cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(${TILED_LIBRARY})

set(SRC_INCLUDES            "../../include")
set(CUSTOM_RENDER_INCLUDE ".")
set(CUSTOM_RENDER_LIB_SRC ".")
set(THIRD_PARTY                 ${CMAKE_SOURCE_DIR}/third-party)
set(LUA_INCLUDE                 ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(PLUGIN_HELPER_SRC          "${CUSTOM_RENDER_LIB_SRC}/../plugin-helper")

 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

add_definitions(-DPLUGIN_CALLBACK)
message(STATUS "Building ${TILED_LIBRARY} lib...")
file(GLOB_RECURSE RENDERER_SOURCES        "${CUSTOM_RENDER_LIB_SRC}/*.cpp" "${PLUGIN_HELPER_SRC}/*.cpp")

#Bring the headers, such as xxx.h into the project
include_directories(${CUSTOM_RENDER_INCLUDE} 
                    ${LUA_INCLUDE}
                    ${SRC_INCLUDES}
                    ${PLUGIN_HELPER_SRC}
                    ${THIRD_PARTY})
 
# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${TILED_LIBRARY} SHARED ${RENDERER_SOURCES})

target_link_libraries(${TILED_LIBRARY} ${LUA_LIBRARY})
target_link_libraries(${TILED_LIBRARY} ${CORE_MBM_LIBRARY})