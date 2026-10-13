cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(${BOX2D_LUA_LIBRARY})

set(CORE_MBM_INCLUDES     ${CMAKE_SOURCE_DIR}/include)
set(PLUGIN_HELPER_INC     ${CMAKE_SOURCE_DIR}/plugins)
set(THIRD_PARTY           ${CMAKE_SOURCE_DIR}/third-party)
set(LUA_INCLUDE           ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(BOX2D_PATH      	    ${CMAKE_SOURCE_DIR}/third-party/box2d-${BOX2DLIB_VERSION})
set(BOX2D_INCLUDE         ${BOX2D_PATH}/include)
set(BOX2D_LUA_SOURCE      ${CMAKE_SOURCE_DIR}/plugins/box2d)
set(MBM_CORE_INCLUDE      ${CORE_MBM_INCLUDES}/core_mbm)

 
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
include_directories(  ${LUA_INCLUDE}
                      ${CORE_MBM_INCLUDES}
                      ${PLUGIN_HELPER_INC}
                      ${BOX2D_INCLUDE}
                      ${MBM_CORE_INCLUDE})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${BOX2D_PATH}/src/*.cpp")
list(APPEND SOURCES box2d-lua.cpp box-2d-wrap.cpp physics-box-2d-lua.cpp physics-box2d-joint-lua.cpp)
if(USE_BOX2D_PARTICLES)
    list(APPEND SOURCES physics-box-2d-fluid-lua.cpp physics-box-2d-fluid-create-lua.cpp)
else()
    list(FILTER SOURCES EXCLUDE REGEX "/src/particle/")
endif()

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS" OR MAS_DELIVERY)
    add_library(${BOX2D_LUA_LIBRARY} STATIC ${SOURCES})
else()
    add_library(${BOX2D_LUA_LIBRARY} SHARED ${SOURCES})
endif()

target_compile_definitions(${BOX2D_LUA_LIBRARY} PUBLIC MBM_BOX2D_PARTICLES=$<BOOL:${USE_BOX2D_PARTICLES}>)
# A forwarding module has no physics sources. Both require orders resolve to the
# same shared library (or the same archive on embedded platforms).
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS" OR MAS_DELIVERY OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
    target_sources(${BOX2D_LUA_LIBRARY} PRIVATE box2d-compat-lua.cpp)
else()
    add_library(box2dLiquidFun SHARED box2d-compat-lua.cpp)
    target_link_libraries(box2dLiquidFun PRIVATE ${BOX2D_LUA_LIBRARY} ${LUA_LIBRARY})
endif()

# On iOS/MAS this is a STATIC archive absorbed into mini-mbm; Lua/core_mbm/plugin_helper
# are provided by mini-mbm at final link — skip to avoid duplicate-library warnings.
if (NOT (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS" OR MAS_DELIVERY))
    target_link_libraries(${BOX2D_LUA_LIBRARY} PRIVATE ${LUA_LIBRARY})
    target_link_libraries(${BOX2D_LUA_LIBRARY} PRIVATE ${CORE_MBM_LIBRARY})
    #target_link_libraries(${BOX2D_LUA_LIBRARY} mini-mbm)
    target_link_libraries(${BOX2D_LUA_LIBRARY} PRIVATE ${PLUGIN_HELPER_LIBRARY})
endif()

# On Windows with MinGW/GCC, B2_API defaults to dllimport unless BOX_2D_BUILD_DLL
# is defined.  Since box2d sources are compiled directly into this shared library
# (not as a separate DLL), force dllexport so static-data-member definitions compile.
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
    target_compile_definitions(${BOX2D_LUA_LIBRARY} PRIVATE BOX_2D_BUILD_DLL)
endif()

