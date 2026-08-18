cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(testLib)

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

if(USE_OPENGL_ES)
  include(FindOpenGLES2)
endif()

#pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(INCLUDE                     ${CMAKE_SOURCE_DIR}/include)
message("INCLUDE: ${INCLUDE}")
set(THIRD_PARTY                 ${CMAKE_SOURCE_DIR}/third-party)
set(RENDER_INCLUDES             ${INCLUDE}/render)
set(CORE_MBM_INCLUDES           ${INCLUDE}/core_mbm)


#Bring the headers, such as xxx.h into the project
if(USE_OPENGL_ES AND OPENGLES2_INCLUDE_DIR AND NOT OPENGLES2_INCLUDE_DIR STREQUAL "OPENGLES2_INCLUDE_DIR-NOTFOUND")
    include_directories(${OPENGLES2_INCLUDE_DIR})
endif()
include_directories(${INCLUDE}
                    ${CORE_MBM_INCLUDES}
                    ${GLES_INCLUDE}
                    ${RENDER_INCLUDES}
                    ${CORE_OPENGLES_INCLUDES}
                    ${CMAKE_SOURCE_DIR}/src/core_mbm
                    ${CMAKE_SOURCE_DIR}/src/core_mbm/private
                    ${THIRD_PARTY})

file(GLOB SOURCES "*.cpp")

# Foundation tests intentionally exercise private, non-exported mesh/skeletal
# helpers. Keep this list aligned with platform-msvs/libTest/libTest.vcxproj;
# exporting these test internals from core_mbm would widen the DLL API solely
# for the harness.
list(APPEND SOURCES
  "${CMAKE_SOURCE_DIR}/src/core_mbm/mesh-v11-io.cpp"
  "${CMAKE_SOURCE_DIR}/src/core_mbm/private/skeletal-animation-foundation.cpp"
  "${CMAKE_SOURCE_DIR}/src/core_mbm/private/skeletal-gpu-lbs.cpp"
  "${CMAKE_SOURCE_DIR}/src/core_mbm/private/skeletal-render-capability.cpp")

if(NOT USE_OPENGL_ES)
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gles-skeletal-parity-tests.cpp")
endif()

add_executable(testLib ${SOURCES})

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

  target_link_libraries(testLib ${X11_LIBRARIES})
  target_link_libraries(testLib ${OPENGLES2_LIBRARY})
  target_link_libraries(testLib ${EGL_LIBRARY})
  target_link_libraries(testLib ${CORE_MBM_LIBRARY})
  target_link_libraries(testLib dl)
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
  if(USE_OPENGL_ES)
    target_link_libraries(testLib ${LIB_EGL_HELD})
    target_link_libraries(testLib ${LIB_GLESv2_HELD})
  endif()
elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
  target_link_libraries(testLib ${X11_LIBRARIES})
  target_link_libraries(testLib ${OPENGLES2_LIBRARY})
  target_link_libraries(testLib ${VULKAN_LIBRARY})
  target_link_libraries(testLib ${EGL_LIBRARY})
  target_link_libraries(testLib ${CORE_MBM_LIBRARY})
else()
  message( SEND_ERROR "Not configurated for ${ENGINE_TARGET_PLATFORM} src/test-lib/CMakeList.txt")
endif()

target_link_libraries(testLib core_mbm)
