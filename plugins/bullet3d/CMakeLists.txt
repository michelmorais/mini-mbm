cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(${BULLET_3D_LIBRARY})

set(CORE_MBM_INCLUDES     "../../include")
set(PLUGIN_HELPER_INC     "../plugin-helper")
set(THIRD_PARTY           ${CMAKE_SOURCE_DIR}/third-party)
set(BULLET3D_PATH      	  ${THIRD_PARTY}/bullet-${BULLET_LIB_VERSION})
set(LUA_INCLUDE           ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(BULLET3D_INCLUDE      "${BULLET3D_PATH}")
set(BULLET3D_SOURCE       "${BULLET3D_PATH}")
set(BULLET3D_PLUGIN_SOURCE   "./")
 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess -Wno-unused-function -Wno-uninitialized -Wno-misleading-indentation")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
endif()

#Bring the headers, such as xxx.h into the project
include_directories(  ${BULLET3D_INCLUDE} 
                      ${BULLET3D_PLUGIN_SOURCE} 
                      ${LUA_INCLUDE}
                      ${CORE_MBM_INCLUDES}
                      ${PLUGIN_HELPER_INC})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES "${BULLET3D_SOURCE}/*.cpp" "${BULLET3D_PLUGIN_SOURCE}/*.cpp" "${PLUGIN_HELPER_INC}/*.cpp")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${BULLET_3D_LIBRARY} SHARED ${SOURCES} )

target_link_libraries(${BULLET_3D_LIBRARY} ${LUA_LIBRARY})
target_link_libraries(${BULLET_3D_LIBRARY} ${CORE_MBM_LIBRARY})

