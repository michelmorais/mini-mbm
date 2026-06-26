cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(mesh_legacy_converter)

# Standalone CLI tool for mesh_deprecated::convertLegacyMeshToV11 (docs/mesh-v11-plan.md Phase B2).
# Offline-only migration tool - never linked into the game runtime.

set(INCLUDE             ${CMAKE_SOURCE_DIR}/include)
set(MESH_DEPRECATED_DIR ${CMAKE_SOURCE_DIR}/src/mesh_deprecated)

include_directories(${INCLUDE}/core_mbm
                    ${INCLUDE}
                    ${MESH_DEPRECATED_DIR})

add_executable(mesh_legacy_converter main.cpp)
target_link_libraries(mesh_legacy_converter PRIVATE mesh_deprecated)
