cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(${BOX2D_LIQUID_FUN_LIBRARY})

#DO NOT include this cmake, it is here in case you want to make the library not together with the wrapper (LUA)

set(BOX2D_LIQUID_FUN_INCLUDE ".")
set(BOX2D_LIQUID_FUN_SRC "./Box2D")
 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
endif()

#Bring the headers, such as xxx.h into the project
include_directories(${BOX2D_LIQUID_FUN_INCLUDE})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES "${BOX2D_LIQUID_FUN_SRC}/*.cpp")
 
#Generate the shared library from the sources
add_library(${BOX2D_LIQUID_FUN_LIBRARY} SHARED ${SOURCES})

