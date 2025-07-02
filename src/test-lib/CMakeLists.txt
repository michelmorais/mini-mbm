cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(testLib)

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

include(FindOpenGLES2)

#pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(INCLUDE                     ${CMAKE_SOURCE_DIR}/include)
message("INCLUDE: ${INCLUDE}")
set(THIRD_PARTY                 ${CMAKE_SOURCE_DIR}/third-party)
set(RENDER_INCLUDES             ${INCLUDE}/render)
set(CORE_MBM_INCLUDES           ${INCLUDE}/core_mbm)

 
#defines
add_definitions(-DUSE_DEPRECATED_2_MINOR)

#Bring the headers, such as xxx.h into the project
include_directories(${OPENGLES2_INCLUDE_DIR}
                    ${INCLUDE}
                    ${GLES_INCLUDE}
                    ${RENDER_INCLUDES}
                    ${CORE_OPENGLES_INCLUDES}
                    ${THIRD_PARTY})

file(GLOB SOURCES "*.cpp")

add_executable(testLib ${SOURCES})


if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

  target_link_libraries(testLib ${X11_LIBRARIES})
  target_link_libraries(testLib ${OPENGLES2_LIBRARY})
  target_link_libraries(testLib ${EGL_LIBRARY})
  target_link_libraries(testLib ${CORE_MBM_LIBRARY})
  target_link_libraries(testLib dl)
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
  target_link_libraries(testLib ${LIB_EGL_HELD})
  target_link_libraries(testLib ${LIB_GLESv2_HELD})
  target_link_libraries(testLib ${LIB_audiere_HELD})
else()
  message( SEND_ERROR "Not configurated for ${ENGINE_TARGET_PLATFORM} src/test-lib/CMakeList.txt")
endif()

target_link_libraries(testLib core_mbm)


