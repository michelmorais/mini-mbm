cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(AUDIERE_LIBRARY)

# On Windows, audiere ships as a pre-built DLL/import-lib in bin/ and lib/.
# The Linux "build-from-source" logic below does not apply — skip it entirely.
if(ENGINE_TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS "Audiere: using pre-built Windows DLL from third-party/audiere-1.9.4/")
    return()
endif()

# Audiere for Linux (trick)
# Since it has it own MakeFile (not converted to cmake yet), we copy the generated library and added as IMPORTED library

set(lib_auriere_version "libaudiere-1.10.1.so")

get_filename_component(PATH_BIN
                       "${FOLDER_BIN}/${FOLDER_ARCHITECTURE}"
                       ABSOLUTE)

get_filename_component(AUDIERE_LIB
                      "${CMAKE_CURRENT_LIST_DIR}/src/.libs/${lib_auriere_version}"
                       ABSOLUTE)

get_filename_component(AUDIERE_USR_LIB
                      "/usr/local/lib/${lib_auriere_version}"
                       ABSOLUTE)

get_filename_component(AUDIERE_PATH
                       "${CMAKE_CURRENT_LIST_DIR}"
                        ABSOLUTE)
 
message(CHECK_START "Finding ${lib_auriere_version}")

if (EXISTS "${AUDIERE_USR_LIB}")
        message(CHECK_START "Found audiere at ${AUDIERE_USR_LIB}")
        set(AUDIERE_LIB ${AUDIERE_USR_LIB})
elseif(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/.libs/${lib_auriere_version}")
        message("******************************************************************************************************")
        message("                                  Audiere                                                             ")
        message("CHECK_FAIL" " - ${lib_auriere_version} not found")
        message( "To build audiere go to: ${AUDIERE_PATH}")
        message("sudo apt-get install osspd")
        message("Type:  ./configure && make")
        message("")
        message("one command line:")
        message("sudo apt-get install osspd && cd \"${AUDIERE_PATH}\" && chmod 777 configure && ./configure && make && cd - && echo \"Success!\"")
        message("")
        message("******************************************************************************************************")
endif()

add_custom_target(
        audiere_cp
        COMMAND cmake -E copy "${AUDIERE_LIB}" "${PATH_BIN}/${lib_auriere_version}"
)
find_program(LS ls HINTS /bin)
if (LS)
        add_custom_target(ls_audiere
                COMMAND cmake -E copy "${AUDIERE_LIB}" "${PATH_BIN}/${lib_auriere_version}"
                COMMAND ${LS} -lSh "${PATH_BIN}/${lib_auriere_version}"
                DEPENDS audiere_cp)

else()
        message("NOT found ls command!")
endif()
