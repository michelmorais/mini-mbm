cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(SQLITE3_LUA_LIBRARY)

set(SRC_INCLUDES            "../../include")
set(SQLITE_INCLUDE ".")
set(SQLITE_SRC ".")
set(LUA_INCLUDE "../lua-${LUA_LIB_VERSION}")
set(SQLITE3_LIBRARY "sqlite3")
 
#defines
if(${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
    add_definitions(-DLUA_COMPAT_ALL -DLUA_ANSI -DLUA_BUILD_AS_DLL)
elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
    add_definitions(-DLUA_COMPAT_ALL -DLUA_ANSI -DLUA_USE_LINUX)
elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
    add_definitions(-DLUA_COMPAT_ALL -DLUA_ANSI -DLUA_USE_LINUX)
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")
    message(STATUS " CMAkE sqlite3 Configuration for Apple (not working yet!!!) is set...")
endif()

add_definitions(-DSQLITE_CORE) #statically link extension of sqlite

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()

#Bring the headers, such as xxx.h into the project
include_directories(${SQLITE_INCLUDE} ${SRC_INCLUDES} ${LUA_INCLUDE})

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    ####
endif()

file(GLOB SOURCES "${SQLITE_SRC}/*.c" "${SQLITE_SRC}/*.cpp")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the static library from the sources
add_library(${SQLITE3_LUA_LIBRARY} SHARED ${SOURCES} )

target_link_libraries(${SQLITE3_LUA_LIBRARY} ${LUA_LIBRARY})
target_link_libraries(${SQLITE3_LUA_LIBRARY} ${CORE_MBM_LIBRARY})