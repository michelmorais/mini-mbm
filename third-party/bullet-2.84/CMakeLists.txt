cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(${BULLET_3D_LIBRARY})

set(BULLET3D_INCLUDE ".")
set(BULLET3D_SRC ".")
 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PTHREADS -pthread -DSCE_PFX_USE_SIMD_VECTORMATH -Wunused-function -Wunused-variable -Wno-strict-aliasing -Wno-unused-function -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PTHREADS -pthread -DSCE_PFX_USE_SIMD_VECTORMATH -Wunused-function -Wunused-variable -Wno-strict-aliasing -Wno-unused-function -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PTHREADS -pthread -DSCE_PFX_USE_SIMD_VECTORMATH -Wunused-function -Wunused-variable -Wno-strict-aliasing -Wno-unused-function -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

#Bring the headers, such as xxx.h into the project
include_directories(${BULLET3D_INCLUDE})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES "${BULLET3D_SRC}/*.cpp")
 
#Generate the static library from the sources
add_library(${BULLET_3D_LIBRARY} SHARED ${SOURCES})

#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#  target_compile_definitions(bullet3d PUBLIC /w4701)
#endif()

