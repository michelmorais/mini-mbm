cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(mesh_deprecated)

# Offline-only legacy mesh (v1-v10) import library (docs/mesh-v11-plan.md Scope Decision 1).
# Never linked into the game runtime - consumed only by a future migration tool/editor.

set(INCLUDE             ${CMAKE_SOURCE_DIR}/include)
set(CORE_MBM_INCLUDES   ${INCLUDE}/core_mbm)
set(THIRD_PARTY         ${CMAKE_SOURCE_DIR}/third-party)

include_directories(${CORE_MBM_INCLUDES}
                    ${INCLUDE}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${THIRD_PARTY})

file(GLOB SOURCES "*.cpp")

if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS" OR MAS_DELIVERY)
    add_library(mesh_deprecated STATIC ${SOURCES})
else()
    add_library(mesh_deprecated SHARED ${SOURCES})
endif()

target_link_libraries(mesh_deprecated PRIVATE core_mbm)
