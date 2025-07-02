# Port audio is linked directly with libcore_mbm , if you want to make a wrapper uncomment this file and include it on main cmake folder
#
# cmake_minimum_required(VERSION 3.0)
# cmake_policy(SET CMP0054 NEW)
# project(${PORT_AUDIO_WRAPPER_LIBRARY})
# 
# set(PORT_AUDIO_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# set(PORT_AUDIO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
# 
#  
# if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-variable")
# elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess")
# elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
# endif()
# 
# #target_include_directories(${PORT_AUDIO_WRAPPER_LIBRARY} PRIVATE ${PORT_AUDIO_INCLUDE})
# #Bring the headers, such as xxx.h into the project
# include_directories(${PORT_AUDIO_INCLUDE})
# 
# find_path(HEADER_PORT_AUDIO portaudio.h)
# if(NOT HEADER_PORT_AUDIO)
#   message( SEND_ERROR "portaudio.h not found. try\n apt-get install portaudio19-dev")
# endif()
# 
# find_library(PORT_AUDIO_LIBRARY portaudio)
# if(NOT PORT_AUDIO_LIBRARY)
#   message( SEND_ERROR "portaudio not found. try\n apt-get install portaudio19-dev")
# endif()
#  
# #However, the file(GLOB...) allows for wildcard additions:
# file(GLOB_RECURSE SOURCES "${PORT_AUDIO_SRC}/*.cpp")
# 
# include_directories(${HEADER_PORT_AUDIO})
# #Generate the shared library from the sources
# add_library(${PORT_AUDIO_WRAPPER_LIBRARY} SHARED ${SOURCES})
# target_link_libraries(${PORT_AUDIO_WRAPPER_LIBRARY} ${PORT_AUDIO_LIBRARY})
# 
# 