cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)
project(mini-mbm)

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake-modules/")

set(INCLUDE                     ${CMAKE_SOURCE_DIR}/include)
set(THIRD_PARTY                 ${CMAKE_SOURCE_DIR}/third-party)
set(PLUGINS                     ${CMAKE_SOURCE_DIR}/plugins)
set(CORE_MBM                    ${INCLUDE}/core_mbm)
set(BOX2D_WRAP_SRC              box-2d-wrap)
set(BULLET3d_WRAP_SRC           bullet-3d-wrap)
set(BOX2D_INC                   ${THIRD_PARTY}/box2d-${BOX2D_LIB_VERSION})
set(BULLET_3D_INC               ${THIRD_PARTY}/bullet-${BULLET_LIB_VERSION})
set(RENDER_INCLUDES             ${INCLUDE}/render)
set(CORE_OPENGLES_INCLUDES      ${INCLUDE}/core_mbm)
set(LUA_INCLUDE                 ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(LUA_WRAP                    ${INCLUDE}/lua-interface)
set(TINYFILEDIALOGS_DIR         ${THIRD_PARTY}/tinyfiledialogs)


if (USE_LUA)

    message(STATUS "Using LUA script...")
    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-linux/main-lua.cpp)
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main.cpp)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(GLES_INCLUDE     ${THIRD_PARTY}/gles)
        endif()
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-android/main-lua.cpp)
    endif()

else()

    message(STATUS " Using pure CPP...")
    if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-linux/main.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-linux/my-scene.cpp)
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-msvs/mini-mbm/main.cpp)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(GLES_INCLUDE     ${THIRD_PARTY}/gles)
        endif()
    elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")
        set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-android/main.cpp)
        set(SCENE_CPP                   ${CMAKE_SOURCE_DIR}/platform-android/scene-1.cpp)
    endif()

endif()

file(GLOB_RECURSE TINYFILEDIALOGS_SOURCES   "${TINYFILEDIALOGS_DIR}/*.c")

#Bring the headers, such as xxx.h into the project
include_directories(${OPENGLES2_INCLUDE_DIR} 
                    ${INCLUDE}
                    ${GLES_INCLUDE}
                    ${RENDER_INCLUDES} 
                    ${CORE_OPENGLES_INCLUDES} 
                    ${THIRD_PARTY}
                    ${LUA_WRAP}
                    ${LUA_INCLUDE}
                    ${BOX2D_INC}
                    ${BULLET_3D_INC}
                    ${PLUGINS})

 
if (USE_LUA)
    file(GLOB_RECURSE SOURCES "lua-wrap/*.cpp")

    if(NOT USE_VR)
        get_filename_component(VR_LUA_CPP "lua-wrap/render-table/vr-lua.cpp" ABSOLUTE)
        list(REMOVE_ITEM SOURCES ${VR_LUA_CPP})
    endif()
endif()

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    #plugins must be added definitions here
    if(USE_LSQLITE3)
        add_definitions(-DUSE_LSQLITE3)
    endif()

    if(USE_BOX2D)
        add_definitions(-DUSE_BOX2D)
    endif()

    if(USE_BULLET3D)
        add_definitions(-DUSE_BULLET3D)
    endif()

    if(USE_IMGUI)
        add_definitions(-DUSE_IMGUI)
    endif()

    add_library(mini-mbm  SHARED  ${MAIN}
                            ${SOURCES}
                            ${BOX2D_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${SCENE_CPP})

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")

    add_executable(mini-mbm ${MAIN}
                        ${SOURCES}
                        ${BOX2D_WRAP_SOURCES}
                        ${BULLET3D_WRAP_SOURCES}
                        ${TINYFILEDIALOGS_SOURCES}
                        ${SCENE_CPP})
    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})

elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")

    add_executable(mini-mbm ${MAIN}
                        ${SOURCES}
                        ${BOX2D_WRAP_SOURCES}
                        ${BULLET3D_WRAP_SOURCES}
                        ${TINYFILEDIALOGS_SOURCES}
                        ${SCENE_CPP})

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "Apple")
    message( SEND_ERROR "Not configurated for Apple src/CMakeList.txt")
endif()

if (USE_LUA)
    target_link_libraries(mini-mbm ${LUA_LIBRARY})
endif()

# plugins for android (https://developer.android.com/about/versions/nougat/android-7.0-changes.html#ndk) will be linked (workaround)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "Android")

    if(USE_LSQLITE3)
        target_link_libraries(mini-mbm ${SQLITE3_LUA_LIBRARY})
    endif()

    if(USE_BOX2D)
        target_link_libraries(mini-mbm ${BOX2D_LIBRARY})
    endif()

    if(USE_BULLET3D)
        target_link_libraries(mini-mbm ${BULLET_3D_LIBRARY})
    endif()

    if(USE_IMGUI)
        target_link_libraries(mini-mbm ${IMGUI_LIBRARY})
    endif()

endif()

get_filename_component(PATH_BIN
                       "${FOLDER_BIN}/${FOLDER_ARCHITECTURE}"
                       ABSOLUTE)
get_filename_component(PATH_LUA
                       "${ROOT_FOLDER}/lua-scripts"
                       ABSOLUTE)
get_filename_component(PATH_LUA_EDITOR
                       "${ROOT_FOLDER}/editor"
                       ABSOLUTE)




if (${ENGINE_TARGET_PLATFORM} STREQUAL "Linux")
    set(REDIRECT_SHELL                    "'>>'")
    if(USE_LUA)
        add_custom_target(
        shortcuts ALL
        DEPENDS shortcuts mini-mbm
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "binaries created at ${PATH_BIN}"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "Creating alias..."
        COMMAND cmake -E echo "alias for scene_editor"
        COMMAND cmake -E echo "alias for sprite_maker"
        COMMAND cmake -E echo "alias for font_maker"
        COMMAND cmake -E echo "alias for shader_editor"
        COMMAND cmake -E echo "alias for particle_editor"
        COMMAND cmake -E echo "alias for physic_editor"
        COMMAND cmake -E echo "alias for mini-mbm"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "alias lua='${PATH_BIN}/lua-${LUA_LIB_VERSION}.exe'" > ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias scene_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/scene_editor2d.lua -w 1920 -h 1080 --nosplash --name scene_editor2d'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias sprite_maker='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/sprite_maker.lua -w 1920 -h 1080 --nosplash --name sprite_maker'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias font_maker='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/font_maker.lua -w 1920 -h 1080 --nosplash --name font_maker'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias shader_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/shader_editor.lua -w 1920 -h 1080 --nosplash --name shader_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias particle_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/particle_editor.lua -w 1920 -h 1080 --nosplash --name particle_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias physic_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/physic_editor.lua -w 1920 -h 1080 --nosplash --name physic_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias asset_packager='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/asset_packager.lua -w 1920 -h 1080 --nosplash --name asset_packager'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias tilemap_editor='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/tilemap_editor.lua -w 1920 -h 1080 --nosplash --name tilemap_editor'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias texture_packer='${PATH_BIN}/mini-mbm --scene ${PATH_LUA_EDITOR}/texture_packer.lua -w 1920 -h 1080 --nosplash --name texture_packer'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "alias minimbm='${PATH_BIN}/mini-mbm -w 1440 -h 600 '" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_PATH='\"?;?.lua;${PATH_BIN}/?.lua\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_CPATH='\"?;?.so;${PATH_BIN}/?.so\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LD_LIBRARY_PATH=${PATH_BIN}:/usr/local/lib:/usr/lib/x86_64-linux-gnu/" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "aliases created at ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "add ${PATH_BIN}/shortcuts.sh at ~/.bashrc to make your shortcut"
        COMMAND cmake -E echo "do:"
        COMMAND cmake -E echo "chmod 755 ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "echo \"source ${PATH_BIN}/shortcuts.sh \"  ${REDIRECT_SHELL} ~/.bashrc"
        COMMAND cmake -E echo ""
        COMMAND cmake -E echo "---------------------------------------------"
        )
    else()
    add_custom_target(
        shortcuts ALL
        DEPENDS shortcuts mini-mbm
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "binaries created at ${PATH_BIN}"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "Creating alias..."
        COMMAND cmake -E echo "alias for mini-mbm"
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "alias minimbm='${PATH_BIN}/mini-mbm'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_PATH='\"?;?.lua;${PATH_BIN}/?.lua\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LUA_CPATH='\"?;?.so;${PATH_BIN}/?.so\"'" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "export LD_LIBRARY_PATH=${PATH_BIN}:/usr/local/lib:/usr/lib/x86_64-linux-gnu/" >> ${PATH_BIN}/shortcuts.sh
        COMMAND cmake -E echo "---------------------------------------------"
        COMMAND cmake -E echo "aliases created at ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "add ${PATH_BIN}/shortcuts.sh at ~/.bashrc to make your shortcut"
        COMMAND cmake -E echo "do:"
        COMMAND cmake -E echo "chmod 755 ${PATH_BIN}/shortcuts.sh"
        COMMAND cmake -E echo "echo \"source ${PATH_BIN}/shortcuts.sh \"  ${REDIRECT_SHELL} ~/.bashrc"
        COMMAND cmake -E echo ""
        COMMAND cmake -E echo "---------------------------------------------")
    endif()
endif()

