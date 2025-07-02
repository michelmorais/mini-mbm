cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)

project(${MODULE_TYNI_OBJ_PARSER})

set(LUA_INCLUDE "../../third-party/lua-${LUA_LIB_VERSION}")
set(MODULES_SRC "../../modules")

add_definitions(-DOBJ_IMP_BUILD_DLL) #instruct to build DLL on windows (see obj_importer_lua.h)
include_directories(${LUA_INCLUDE} ${MODULES_SRC})


#the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES_TYNI_OBJ_PARSER_C   "./*.cc")
file(GLOB SOURCES_TYNI_OBJ_PARSER_CPP "./*.cpp")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${MODULE_TYNI_OBJ_PARSER}  SHARED ${SOURCES_TYNI_OBJ_PARSER_C} ${SOURCES_TYNI_OBJ_PARSER_CPP})

target_link_libraries(${MODULE_TYNI_OBJ_PARSER} ${LUA_LIBRARY})