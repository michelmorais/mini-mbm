cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(${STEAM_LIBRARY})

set(SRC_INCLUDES        ${CMAKE_SOURCE_DIR}/include)
set(LUA_WRAP            ${CMAKE_SOURCE_DIR}/include/lua-interface)
set(PLUGIN_HELPER_INC   ${CMAKE_SOURCE_DIR}/plugins)
set(STEAM_PLUGIN_SRC    ${CMAKE_SOURCE_DIR}/plugins/steam)
set(THIRD_PARTY         ${CMAKE_SOURCE_DIR}/third-party)
set(LUA_INCLUDE         ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})

message(STATUS "Building Steam plugin, SDK at: ${STEAMWORKS_SDK_PATH}")

file(GLOB_RECURSE STEAM_SOURCES "${STEAM_PLUGIN_SRC}/*.cpp")

#Bring the headers into the project
include_directories(
    ${STEAM_PLUGIN_SRC}
    ${LUA_INCLUDE}
    ${SRC_INCLUDES}
    ${SRC_INCLUDES}/core_mbm
    ${LUA_WRAP}
    ${PLUGIN_HELPER_INC}
    ${STEAMWORKS_SDK_PATH}/public
)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Generate the shared library from the sources (static on iOS)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    add_library(${STEAM_LIBRARY} STATIC ${STEAM_SOURCES})
else()
    add_library(${STEAM_LIBRARY} SHARED ${STEAM_SOURCES})
endif()

# Platform-specific Steam SDK import library
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
    target_compile_definitions(${STEAM_LIBRARY} PRIVATE STEAM_PLUGIN_BUILD_DLL)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64-bit
        target_link_libraries(${STEAM_LIBRARY} PRIVATE
            "${STEAMWORKS_SDK_PATH}/redistributable_bin/win64/steam_api64.lib")
    else()
        # 32-bit
        target_link_libraries(${STEAM_LIBRARY} PRIVATE
            "${STEAMWORKS_SDK_PATH}/redistributable_bin/steam_api.lib")
    endif()
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
    target_link_libraries(${STEAM_LIBRARY} PRIVATE
        "${STEAMWORKS_SDK_PATH}/redistributable_bin/linux64/libsteam_api.so")
    # Copy libsteam_api.so next to the engine binary after every build
    add_custom_command(TARGET ${STEAM_LIBRARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STEAMWORKS_SDK_PATH}/redistributable_bin/linux64/libsteam_api.so"
            "$<TARGET_FILE_DIR:${STEAM_LIBRARY}>/libsteam_api.so"
        COMMENT "Copying libsteam_api.so to output directory"
    )
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "MacOs")
    target_link_libraries(${STEAM_LIBRARY} PRIVATE
        "${STEAMWORKS_SDK_PATH}/redistributable_bin/osx/libsteam_api.dylib")
    # Copy libsteam_api.dylib next to the engine binary after every build
    add_custom_command(TARGET ${STEAM_LIBRARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STEAMWORKS_SDK_PATH}/redistributable_bin/osx/libsteam_api.dylib"
            "$<TARGET_FILE_DIR:${STEAM_LIBRARY}>/libsteam_api.dylib"
        COMMENT "Copying libsteam_api.dylib to output directory"
    )
endif()

# On iOS the static library is absorbed by the final mini-mbm link.
# Lua, plugin_helper and core_mbm are therefore provided at that stage.
if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    target_link_libraries(${STEAM_LIBRARY} PRIVATE ${LUA_LIBRARY})
    target_link_libraries(${STEAM_LIBRARY} PRIVATE ${PLUGIN_HELPER_LIBRARY})
    target_link_libraries(${STEAM_LIBRARY} PRIVATE ${CORE_MBM_LIBRARY})
endif()
