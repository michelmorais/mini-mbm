cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(${BULLET_3D_LIBRARY})

set(CORE_MBM_INCLUDES                 "../../include")
set(PLUGIN_HELPER_INC                 ${CMAKE_SOURCE_DIR}/plugins)
set(THIRD_PARTY                       ${CMAKE_SOURCE_DIR}/third-party)
set(BULLET3D_PATH      	              ${THIRD_PARTY}/bullet-${BULLET_LIB_VERSION})
set(LUA_INCLUDE                       ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(BULLET3D_INCLUDE                  "${BULLET3D_PATH}")
set(BULLET3D_SOURCE                   "${BULLET3D_PATH}")
set(BULLET3D_PLUGIN_SOURCE            "./")
set(USER_DATA_INCLUDE                 ${CORE_MBM_INCLUDES}/lua-interface/lua-wrap)
set(MBM_CORE_INCLUDE                  ${CORE_MBM_INCLUDES}/core_mbm)
 
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
                      ${USER_DATA_INCLUDE}
                      ${MBM_CORE_INCLUDE}
                      ${PLUGIN_HELPER_INC})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES "${BULLET3D_SOURCE}/*.cpp" "${BULLET3D_PLUGIN_SOURCE}/*.cpp" )

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS" OR MAS_DELIVERY)
    add_library(${BULLET_3D_LIBRARY} STATIC ${SOURCES})
else()
    add_library(${BULLET_3D_LIBRARY} SHARED ${SOURCES})
endif()

if (NOT (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS" OR MAS_DELIVERY))
    target_link_libraries(${BULLET_3D_LIBRARY} PRIVATE ${LUA_LIBRARY})
    target_link_libraries(${BULLET_3D_LIBRARY} PRIVATE ${CORE_MBM_LIBRARY})
    target_link_libraries(${BULLET_3D_LIBRARY} PRIVATE ${PLUGIN_HELPER_LIBRARY})
endif()

