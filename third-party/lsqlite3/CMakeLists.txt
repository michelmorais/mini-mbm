cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
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
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    message(STATUS " CMAkE sqlite3 Configuration for Apple (not working yet!!!) is set...")
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    add_definitions(-DLUA_COMPAT_ALL -DLUA_ANSI -DLUA_USE_IOS)
else()
  message(SEND_ERROR "Unknown ENGINE_TARGET_PLATFORM ${ENGINE_TARGET_PLATFORM}")
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
# third-party/ is needed so <dirent-1-13/dirent.h> resolves on Windows/MinGW
include_directories(${SQLITE_INCLUDE} ${SRC_INCLUDES} ${LUA_INCLUDE} "${CMAKE_SOURCE_DIR}/third-party")

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    #pthreads — libc on iOS already includes pthread; REQUIRED avoids link errors
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

file(GLOB SOURCES "${SQLITE_SRC}/*.c" "${SQLITE_SRC}/*.cpp")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
# iOS requires static libraries (App Store sandbox)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    add_library(${SQLITE3_LUA_LIBRARY} STATIC ${SOURCES})
else()
    add_library(${SQLITE3_LUA_LIBRARY} SHARED ${SOURCES})
endif()

# On iOS lsqlite3 is a STATIC archive embedded in mini-mbm. CMake propagates PRIVATE
# static deps to the consumer, duplicating the explicit links in mini-mbm's
# target_link_libraries calls and triggering ld duplicate-library warnings.
# Both Lua and core_mbm are provided by mini-mbm at final link.
if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    target_link_libraries(${SQLITE3_LUA_LIBRARY} PRIVATE ${LUA_LIBRARY})
    target_link_libraries(${SQLITE3_LUA_LIBRARY} PRIVATE ${CORE_MBM_LIBRARY})
endif()