cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(${BOX2D_LIQUID_FUN_LIBRARY})

set(CORE_MBM_INCLUDES             "../../include")
set(PLUGIN_HELPER_INC             "../plugin-helper")
set(THIRD_PARTY                   ${CMAKE_SOURCE_DIR}/third-party)
set(BOX2D_LIQUID_FUN_PATH         ${THIRD_PARTY}/box2d-liquid-fun-${BOX2D_LIQUID_FUN_LIB_VERSION})
set(LUA_INCLUDE                   ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(BOX2D_LIQUID_FUN_ROOT         "${BOX2D_LIQUID_FUN_PATH}")
set(BOX2D_LIQUID_FUN_INCLUDE_LIQUID_FUN      "${BOX2D_LIQUID_FUN_PATH}/include")
set(BOX2D_SOURCE_LIQUID_FUN       "${BOX2D_LIQUID_FUN_PATH}/src")
set(BOX2D_PLUGIN_LIQUID_FUN_SOURCE   "./")
 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
endif()

#set_target_properties(box2d PROPERTIES
#	CXX_STANDARD 11
#    CXX_STANDARD_REQUIRED YES
#    CXX_EXTENSIONS NO
#    VERSION ${PROJECT_VERSION}
#    SOVERSION ${PROJECT_VERSION_MAJOR}
#)
#

#Bring the headers, such as xxx.h into the project
include_directories(  ${BOX2D_LIQUID_FUN_ROOT} 
                      ${BOX2D_LIQUID_FUN_INCLUDE_LIQUID_FUN}
                      ${BOX2D_PLUGIN_LIQUID_FUN_SOURCE}
                      ${THIRD_PARTY}
                      ${LUA_INCLUDE}
                      ${CORE_MBM_INCLUDES}
                      ${PLUGIN_HELPER_INC})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES "${BOX2D_SOURCE_LIQUID_FUN}/*.cpp" "${BOX2D_PLUGIN_LIQUID_FUN_SOURCE}/*.cpp" "${PLUGIN_HELPER_INC}/*.cpp")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${LIQUID_FUN_BOX2D_LIBRARY} SHARED ${SOURCES} )

target_link_libraries(${LIQUID_FUN_BOX2D_LIBRARY} ${LUA_LIBRARY})
target_link_libraries(${LIQUID_FUN_BOX2D_LIBRARY} ${CORE_MBM_LIBRARY})

