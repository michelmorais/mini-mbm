cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)

project(modules)

#Modules
set(MODULE_TYNI_OBJ_PARSER                     "tiny_obj_loader")
set(MODULE_TYNI_OBJ_PARSER_DIR                 "./obj_importer_lua")

add_definitions(-DLUA_COMPAT_ALL -DLUA_ANSI)

#-fPIC : The -fPIC option tells the compiler to generate position independent code (PIC).
# This is needed because the code in a shared library may be loaded into a different memory location
# for different executables.
# So, it's important to generate PIC code for shared libraries so that user code deos not dependent
# on the absolute memory address of symbols.

if (${ENGINE_TARGET_PLATFORM} STREQUAL "Windows")
  add_definitions(-DLUA_BUILD_AS_DLL)
else()
  add_definitions(-DLUA_USE_LINUX)
endif()


#All modules should be generated at bin folder
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

add_subdirectory(${MODULE_TYNI_OBJ_PARSER_DIR} 				${MODULE_TYNI_OBJ_PARSER_DIR})

#example
#module_folder = '/home/path/to/lib'
#package.path = module_folder .. "?.so;" .. package.path
#
#tiny_obj_loader = require "tiny_obj_loader"
#print(tiny_obj_loader)
