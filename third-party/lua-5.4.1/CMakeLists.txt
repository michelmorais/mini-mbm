cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(liblua-${LUA_LIB_VERSION})

set(LUA_SRC ".")
 
#defines
add_definitions(-DLUA_COMPAT_ALL -DLUA_ANSI)

if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
  add_definitions(-DUSE_NATIVE_PRINT_LUA)
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
  add_definitions(-DLUA_BUILD_AS_DLL)
else()
  add_definitions(-DLUA_USE_LINUX)
endif()

#Bring the headers, such as xxx.h into the project
include_directories(${LUA_SRC})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "${LUA_SRC}/*.c")
#remove main() file
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${LUA_SRC}/lua.c")
 
#Generate the shared library from the sources
add_library(lua-${LUA_LIB_VERSION} SHARED ${SOURCES})

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ROOT_FOLDER}/src/cmake-modules/")
  include(FindReadline)

  set(LUA_EXE "lua-${LUA_LIB_VERSION}.exe")

  add_executable(${LUA_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/${LUA_SRC}/lua.c)

  target_link_libraries(${LUA_EXE} lua-${LUA_LIB_VERSION})

  target_link_libraries(${LUA_EXE} readline)
  target_link_libraries(${LUA_EXE} dl)
  target_link_libraries(${LUA_EXE} m)

endif()
                        

