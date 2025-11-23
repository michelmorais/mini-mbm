cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(${PLUGIN_HELPER_LIBRARY})

set(THIRD_PARTY           ${CMAKE_SOURCE_DIR}/third-party)
set(CORE_MBM_INCLUDES     ${CMAKE_SOURCE_DIR}/include)
set(LUA_INCLUDE           ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(MBM_CORE_INCLUDE      ${CORE_MBM_INCLUDES}/core_mbm)
set(PLUGIN_HELPER_INC     .)

 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
endif()


#Bring the headers, such as xxx.h into the project
include_directories(  ${LUA_INCLUDE}
                      ${THIRD_PARTY}
                      ${CORE_MBM_INCLUDES}
                      ${PLUGIN_HELPER_INC}
                      ${MBM_CORE_INCLUDE})
 
#However, the file(GLOB...) allows for wildcard additions:
file(GLOB_RECURSE SOURCES "${PLUGIN_HELPER_INC}/*.cpp" )

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${PLUGIN_HELPER_LIBRARY} SHARED ${SOURCES} )


target_link_libraries(${PLUGIN_HELPER_LIBRARY} ${LUA_LIBRARY})

# For Linux / Windows we have the executable mini-mbm and the library ibcore_mbm.so
# For Android we have libcore_mbm.so and libmini-mbm.so
if(PLAT_LOWER STREQUAL "android")
  #target_link_libraries(${PLUGIN_HELPER_LIBRARY} mini-mbm)
  target_link_libraries(${PLUGIN_HELPER_LIBRARY} ${CORE_MBM_LIBRARY})
else()
  target_link_libraries(${PLUGIN_HELPER_LIBRARY} ${CORE_MBM_LIBRARY})
endif()

