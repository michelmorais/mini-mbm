cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(${IMGUI_LIBRARY})

set(SRC_INCLUDES            ${CMAKE_SOURCE_DIR}/include)
set(LUA_WRAP                ${CMAKE_SOURCE_DIR}/include/lua-interface)
set(PLUGIN_HELPER_INC       ${CMAKE_SOURCE_DIR}/plugins)
set(IMGUI_INCLUDE           ${CMAKE_SOURCE_DIR}/plugins/imGui)
set(IMGUI_SRC               ${CMAKE_SOURCE_DIR}/plugins/imGui)
set(IMGUI_BACKENDS          ${IMGUI_SRC}/backends)
set(THIRD_PARTY             ${CMAKE_SOURCE_DIR}/third-party)
set(LUA_INCLUDE             ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
 
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-char-subscripts")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

message(STATUS "Building IMGUI lib...")
file(GLOB_RECURSE IM_GUI_SOURCES        "${IMGUI_SRC}/*.cpp")

# On Apple/Metal builds, gather .mm sources and apply ARC.
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple" AND USE_METAL)
    file(GLOB_RECURSE IM_GUI_MM_SOURCES "${IMGUI_SRC}/*.mm")
    list(APPEND IM_GUI_SOURCES ${IM_GUI_MM_SOURCES})
    if (IM_GUI_MM_SOURCES)
        set_source_files_properties(${IM_GUI_MM_SOURCES}
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    endif()
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Linux" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_win32.cpp")
endif()

# Compile only the render backend that matches the engine (USE_DIRECTX9 / USE_OPENGL_ES from root CMake)
# For USE_DUMMY_BACK_END_ENGINE, exclude both backends (imgui-lua.cpp uses dummy stubs)
if (USE_DUMMY_BACK_END_ENGINE)
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_opengl3.cpp")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_dx9.cpp")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_win32.cpp")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_metal.mm")
elseif (USE_DIRECTX9)
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_opengl3.cpp")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_metal.mm")
elseif (USE_METAL)
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_opengl3.cpp")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_dx9.cpp")
    # imgui_impl_win32.cpp is already removed above for Apple
    # imgui_impl_metal.mm stays in sources
elseif (USE_OPENGL_ES)
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_dx9.cpp")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_BACKENDS}/imgui_impl_metal.mm")

endif()

# Exclude imgui_demo.cpp in release mode (ShowDemoWindow, ShowStyleSelector, etc. are debug-only)
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" _IMGUI_CMAKE_BUILD_TYPE_LOWER)
if (NOT _IMGUI_CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_SRC}/imgui_demo.cpp")
    message(STATUS "ImGui: excluding imgui_demo.cpp, using imgui_demo_stubs.cpp (release build)")
else()
    list(REMOVE_ITEM IM_GUI_SOURCES "${IMGUI_SRC}/imgui_demo_stubs.cpp")
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux" OR ${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")

    find_package(X11 REQUIRED)

    message(STATUS "X11_FOUND = ${X11_FOUND}")
    message(STATUS "X11_INCLUDE_DIR = ${X11_INCLUDE_DIR}")
    message(STATUS "X11_LIBRARIES = ${X11_LIBRARIES}")

    include_directories(${X11_INCLUDE_DIR})
    ####
  endif()

#Bring the headers, such as xxx.h into the project
include_directories(  ${IMGUI_INCLUDE}
                      ${LUA_INCLUDE}
                      ${SRC_INCLUDES}
                      ${SRC_INCLUDES}/core_mbm
                      ${LUA_WRAP}
                      ${PLUGIN_HELPER_INC}
                      ${IMGUI_BACKENDS}
                      ${THIRD_PARTY})
 
# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

    #Opengl
    include(FindOpenGLES2)
    #pthreads
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    ####

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
  #Opengl
  include(FindOpenGLES2)
  find_package(ndk-stl REQUIRED)#depend on cmake-modules/Findndk-stl.cmake 
endif()
set(CMAKE_SHARED_LIBRARY_PREFIX "")
#Generate the shared library from the sources
add_library(${IMGUI_LIBRARY} SHARED ${IM_GUI_SOURCES})

if (USE_OPENGL_ES)
  target_compile_definitions(${IMGUI_LIBRARY} PRIVATE IMGUI_IMPL_OPENGL_ES2 IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
endif()
if (USE_DIRECTX9 AND ${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
  target_link_libraries(${IMGUI_LIBRARY} d3d9)
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple" AND USE_METAL)
  target_link_libraries(${IMGUI_LIBRARY} "-framework Metal")
  target_link_libraries(${IMGUI_LIBRARY} "-framework Foundation")
  target_link_libraries(${IMGUI_LIBRARY} "-framework QuartzCore")
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
  target_link_libraries(${IMGUI_LIBRARY} ${OPENGLES2_LIBRARY})
endif()

target_link_libraries(${IMGUI_LIBRARY} ${LUA_LIBRARY})
target_link_libraries(${IMGUI_LIBRARY} ${PLUGIN_HELPER_LIBRARY})
target_link_libraries(${IMGUI_LIBRARY} ${CORE_MBM_LIBRARY})
