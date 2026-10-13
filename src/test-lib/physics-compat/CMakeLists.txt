# /*-----------------------------------------------------------------------------------------------------------------------|
# | MIT License (MIT)                                                                                                      |
# | Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
# |                                                                                                                        |
# | Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
# | documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
# | the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
# | to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
# |                                                                                                                        |
# | The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
# | the Software.                                                                                                          |
# |                                                                                                                        |
# | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
# | WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
# | COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# | OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
# |                                                                                                                        |
# |-----------------------------------------------------------------------------------------------------------------------*/

cmake_minimum_required(VERSION 3.25.1)
project(mbm_physics_compat LANGUAGES CXX)

# Standalone native checks: never link both physics implementations together.
if(NOT DEFINED PLAT)
    message(FATAL_ERROR "Pass -DPLAT=Linux|Windows|MacOs|Android|iOS")
endif()
message(STATUS "Physics compatibility checks for ${PLAT}")
get_filename_component(MBM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PHYSICS_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
enable_testing()

foreach(backend IN ITEMS rigid liquidfun candidate)
    if(NOT backend STREQUAL "liquidfun")
        set(source_root "${MBM_ROOT}/third-party/box2d-2.4.1")
        file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${source_root}/src/*.cpp")
        if(backend STREQUAL "rigid")
            list(FILTER sources EXCLUDE REGEX "/src/particle/")
        endif()
        set(include_root "${source_root}/include")
    else()
        set(source_root "${MBM_ROOT}/third-party/box2d-liquid-fun-2.3.1")
        file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${source_root}/Box2D/*.cpp")
        set(include_root "${source_root}")
    endif()
    add_executable(physics_${backend} physics-baseline.cpp ${sources})
    target_include_directories(physics_${backend} PRIVATE "${include_root}")
    target_compile_definitions(physics_${backend} PRIVATE
        PHYSICS_BACKEND="${backend}")
    if(backend STREQUAL "liquidfun")
        target_compile_definitions(physics_${backend} PRIVATE PHYSICS_PARTICLES=1)
    elseif(backend STREQUAL "candidate")
        target_compile_definitions(physics_${backend} PRIVATE PHYSICS_PARTICLES=1 PHYSICS_UNIFIED=1 MBM_BOX2D_PARTICLES=1)
    endif()
    if(WIN32 AND NOT backend STREQUAL "liquidfun")
        target_compile_definitions(physics_${backend} PRIVATE BOX_2D_BUILD_DLL)
    endif()
    if(PHYSICS_SANITIZERS)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR "PHYSICS_SANITIZERS requires GCC or Clang")
        endif()
        target_compile_options(physics_${backend} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(physics_${backend} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME physics_${backend} COMMAND physics_${backend} --check)
    set_tests_properties(physics_${backend} PROPERTIES
        TIMEOUT 30 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
endforeach()

add_test(NAME rigid_particle_off_on_equivalence COMMAND ${CMAKE_COMMAND}
    -DRIGID=$<TARGET_FILE:physics_rigid> -DCANDIDATE=$<TARGET_FILE:physics_candidate>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/compare-rigid.cmake)
set_tests_properties(rigid_particle_off_on_equivalence PROPERTIES
    TIMEOUT 45 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
