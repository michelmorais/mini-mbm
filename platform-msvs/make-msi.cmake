cmake_minimum_required(VERSION 3.25.1)
# ┌─────────────────────────────────────────────────────────────────────────┐
# │ make-msi.cmake  —  standalone WiX MSI generator for the VS build path  │
# │                                                                         │
# │  Usage (called from package-game.bat or any shell):                    │
# │    cmake -DGAME_NAME="Tower Defense Monster"                            │
# │          -DGAME_DIR="C:\path\to\Tower_Defense_Monster.GameDir"          │
# │          -DOUTPUT_DIR="C:\path\to\output"                               │
# │         [-DGAME_NAME_SAFE="Tower_Defense_Monster"]                      │
# │         [-DICON_ICO="C:\path\to\icon.ico"]                              │
# │          -P make-msi.cmake                                              │
# │                                                                         │
# │  Produces: <OUTPUT_DIR>\<GAME_NAME_SAFE>-windows.msi                   │
# └─────────────────────────────────────────────────────────────────────────┘

# ── Validate required inputs ─────────────────────────────────────────────────
if(NOT GAME_NAME)
    message(FATAL_ERROR
        "GAME_NAME not set.\n"
        "Pass -DGAME_NAME=\"My Game\" on the cmake command line.")
endif()
if(NOT GAME_DIR)
    message(FATAL_ERROR
        "GAME_DIR not set.\n"
        "Pass -DGAME_DIR=\"C:\\path\\to\\GameName.GameDir\"")
endif()
if(NOT EXISTS "${GAME_DIR}")
    message(FATAL_ERROR "GAME_DIR does not exist: ${GAME_DIR}")
endif()

# ── Default OUTPUT_DIR to the directory that contains this script ─────────────
if(NOT OUTPUT_DIR)
    set(OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

# ── Derive GAME_NAME_SAFE (spaces and special chars → underscores) ────────────
if(NOT GAME_NAME_SAFE)
    string(REPLACE " " "_" GAME_NAME_SAFE "${GAME_NAME}")
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" GAME_NAME_SAFE "${GAME_NAME_SAFE}")
endif()

set(WIN_GAMEDIR       "${GAME_DIR}")
set(WIX_OUTPUT_WXS    "${OUTPUT_DIR}/${GAME_NAME_SAFE}.wxs")
set(WIX_OUTPUT_MSI    "${OUTPUT_DIR}/${GAME_NAME_SAFE}-windows.msi")
set(WIX_ICON_ICO_PATH "${ICON_ICO}")

# ── Deterministic GUIDs (same algorithm as the CMake configure-time path) ─────
# Using a fixed namespace so the same game always gets the same UpgradeCode,
# which lets Windows Installer detect and upgrade an existing installation.
set(MBM_WIX_NS "6BA7B810-9DAD-11D1-80B4-00C04FD430C8")
string(UUID WIX_UPGRADE_CODE
    NAMESPACE "${MBM_WIX_NS}"
    NAME      "mini-mbm-upgrade-${GAME_NAME_SAFE}"
    TYPE SHA1 UPPER)

# ── Detect WiX ───────────────────────────────────────────────────────────────
# NO_CACHE prevents a previous set("") from hiding the cache result on re-runs.
find_program(WIX_V4 wix NO_CACHE)
if(NOT WIX_V4)
    # Accept any WiX v3.x installation (e.g. v3.11, v3.14, …)
    file(GLOB _WIX3_DIRS
        "C:/Program Files (x86)/WiX Toolset v3*/bin"
        "C:/Program Files/WiX Toolset v3*/bin")
    find_program(WIX_CANDLE candle PATHS ${_WIX3_DIRS} NO_CACHE)
    find_program(WIX_LIGHT  light  PATHS ${_WIX3_DIRS} NO_CACHE)
endif()
if(NOT WIX_V4 AND NOT WIX_CANDLE)
    message(STATUS "WiX not found — skipping MSI generation.")
    message(STATUS "  WiX v4 (recommended): dotnet tool install --global wix")
    message(STATUS "                        https://wixtoolset.org")
    message(STATUS "  WiX v3: https://github.com/wixtoolset/wix3/releases")
    message(STATUS "GameDir ready at: ${WIN_GAMEDIR}")
    return()
endif()

# ── Helpers ───────────────────────────────────────────────────────────────────
function(path_to_id OUT_VAR prefix rel_path)
    # WiX v3 identifiers: only [A-Za-z0-9_.] allowed — replace everything else.
    string(REGEX REPLACE "[^A-Za-z0-9_.]" "_" _id "${rel_path}")
    set(${OUT_VAR} "${prefix}_${_id}" PARENT_SCOPE)
endfunction()

function(make_guid OUT_VAR name)
    string(UUID _g NAMESPACE "${MBM_WIX_NS}"
           NAME "comp-${GAME_NAME_SAFE}-${name}" TYPE SHA1 UPPER)
    set(${OUT_VAR} "{${_g}}" PARENT_SCOPE)
endfunction()

# ── Enumerate all files in the populated GameDir ──────────────────────────────
file(GLOB_RECURSE _ALL_FILES LIST_DIRECTORIES false "${WIN_GAMEDIR}/*")
list(SORT _ALL_FILES)

# ── Build the nested <Directory> + <Component> XML ───────────────────────────
set(_CURRENT_DIR "")
set(WXS_BODY "")
set(WXS_COMP_IDS "")

foreach(_F ${_ALL_FILES})
    file(RELATIVE_PATH _REL "${WIN_GAMEDIR}" "${_F}")
    get_filename_component(_DIR  "${_REL}" DIRECTORY)
    string(REPLACE "\\" "/" _DIR "${_DIR}")
    string(REPLACE "\\" "/" _REL "${_REL}")

    # Transition directories: only close/open at the diverging level so that
    # parent directories shared with the previous path are not re-declared.
    if(NOT "${_DIR}" STREQUAL "${_CURRENT_DIR}")
        if(_CURRENT_DIR)
            string(REPLACE "/" ";" _PREV_PARTS "${_CURRENT_DIR}")
        else()
            set(_PREV_PARTS "")
        endif()
        if(_DIR)
            string(REPLACE "/" ";" _NEXT_PARTS "${_DIR}")
        else()
            set(_NEXT_PARTS "")
        endif()
        list(LENGTH _PREV_PARTS _PREV_LEN)
        list(LENGTH _NEXT_PARTS _NEXT_LEN)
        # Count common leading parts
        set(_COMMON 0)
        set(_MORE TRUE)
        while(_MORE)
            if(_COMMON GREATER_EQUAL _PREV_LEN OR _COMMON GREATER_EQUAL _NEXT_LEN)
                set(_MORE FALSE)
            else()
                list(GET _PREV_PARTS ${_COMMON} _PP)
                list(GET _NEXT_PARTS ${_COMMON} _NP)
                if("${_PP}" STREQUAL "${_NP}")
                    math(EXPR _COMMON "${_COMMON} + 1")
                else()
                    set(_MORE FALSE)
                endif()
            endif()
        endwhile()
        # Close dirs from _CURRENT_DIR down to the common prefix
        math(EXPR _CLOSE "${_PREV_LEN} - ${_COMMON}")
        if(_CLOSE GREATER 0)
            foreach(_I RANGE 1 ${_CLOSE})
                string(APPEND WXS_BODY "            </Directory>\n")
            endforeach()
        endif()
        # Open dirs from the common prefix up to _DIR
        math(EXPR _OPEN_END "${_NEXT_LEN} - 1")
        if(_NEXT_LEN GREATER _COMMON)
            set(_ACC "")
            if(_COMMON GREATER 0)
                math(EXPR _LAST_COMMON "${_COMMON} - 1")
                foreach(_J RANGE 0 ${_LAST_COMMON})
                    list(GET _NEXT_PARTS ${_J} _PART)
                    if(_ACC)
                        set(_ACC "${_ACC}/${_PART}")
                    else()
                        set(_ACC "${_PART}")
                    endif()
                endforeach()
            endif()
            foreach(_J RANGE ${_COMMON} ${_OPEN_END})
                list(GET _NEXT_PARTS ${_J} _PART)
                if(_ACC)
                    set(_ACC "${_ACC}/${_PART}")
                else()
                    set(_ACC "${_PART}")
                endif()
                path_to_id(_DID "Dir" "${_ACC}")
                string(APPEND WXS_BODY "            <Directory Id=\"${_DID}\" Name=\"${_PART}\">\n")
            endforeach()
        endif()
        set(_CURRENT_DIR "${_DIR}")
    endif()

    path_to_id(_CID "Comp" "${_REL}")
    path_to_id(_FID "File" "${_REL}")
    make_guid(_GUID "${_REL}")
    string(APPEND WXS_BODY
        "                <Component Id=\"${_CID}\" Guid=\"${_GUID}\">\n"
        "                    <File Id=\"${_FID}\" Source=\"${_F}\" KeyPath=\"yes\" />\n"
        "                </Component>\n")
    list(APPEND WXS_COMP_IDS "${_CID}")
endforeach()

# Close remaining open directories
if(_CURRENT_DIR)
    string(REPLACE "/" ";" _CLOSE_PARTS "${_CURRENT_DIR}")
    list(LENGTH _CLOSE_PARTS _DEPTH)
    foreach(_I RANGE 1 ${_DEPTH})
        string(APPEND WXS_BODY "            </Directory>\n")
    endforeach()
endif()

# ── Shortcut component ────────────────────────────────────────────────────────
make_guid(_SC_GUID "shortcuts-${GAME_NAME_SAFE}")
set(_SHORTCUT_COMP
"        <DirectoryRef Id=\"ProgramMenuDir\">
            <Component Id=\"Comp_Shortcuts\" Guid=\"${_SC_GUID}\">
                <Shortcut Id=\"StartMenuShortcut\"
                          Name=\"${GAME_NAME}\"
                          Description=\"Play ${GAME_NAME}\"
                          Target=\"[INSTALLDIR]${GAME_NAME_SAFE}.exe\"
                          Arguments=\"--name &quot;${GAME_NAME}&quot; --scene assets\\main.lua\"
                          WorkingDirectory=\"INSTALLDIR\" />
                <Shortcut Id=\"DesktopShortcut\"
                          Name=\"${GAME_NAME}\"
                          Description=\"Play ${GAME_NAME}\"
                          Target=\"[INSTALLDIR]${GAME_NAME_SAFE}.exe\"
                          Arguments=\"--name &quot;${GAME_NAME}&quot; --scene assets\\main.lua\"
                          WorkingDirectory=\"INSTALLDIR\"
                          Directory=\"DesktopFolder\" />
                <Shortcut Id=\"UninstallShortcut\"
                          Name=\"Uninstall ${GAME_NAME}\"
                          Description=\"Uninstall ${GAME_NAME}\"
                          Target=\"[SystemFolder]msiexec.exe\"
                          Arguments=\"/x [ProductCode]\" />
                <RemoveFolder Id=\"RemoveProgramMenuDir\" Directory=\"ProgramMenuDir\" On=\"uninstall\" />
                <RegistryValue Root=\"HKCU\" Key=\"Software\\${GAME_NAME_SAFE}\"
                               Name=\"installed\" Type=\"integer\" Value=\"1\" KeyPath=\"yes\" />
            </Component>
        </DirectoryRef>
")

# ── Component refs ────────────────────────────────────────────────────────────
set(_COMP_REFS "")
foreach(_CID ${WXS_COMP_IDS})
    string(APPEND _COMP_REFS "            <ComponentRef Id=\"${_CID}\" />\n")
endforeach()
string(APPEND _COMP_REFS "            <ComponentRef Id=\"Comp_Shortcuts\" />\n")

# ── Icon XML ──────────────────────────────────────────────────────────────────
set(_WIX_ICON_XML "")
if(NOT "${WIX_ICON_ICO_PATH}" STREQUAL "" AND EXISTS "${WIX_ICON_ICO_PATH}")
    set(_WIX_ICON_XML
        "        <Icon Id=\"AppIcon.ico\" SourceFile=\"${WIX_ICON_ICO_PATH}\" />\n        <Property Id=\"ARPPRODUCTICON\" Value=\"AppIcon.ico\" />")
endif()

# ── License RTF ───────────────────────────────────────────────────────────────
string(REPLACE ".wxs" "_license.rtf" _WIX_LICENSE_RTF "${WIX_OUTPUT_WXS}")
file(WRITE "${_WIX_LICENSE_RTF}"
"{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fswiss\\fcharset0 Arial;}}"
"{\\colortbl ;\\red0\\green0\\blue0;}"
"\\viewkind4\\uc1\\pard\\cf1\\f0\\fs22\\b ${GAME_NAME}\\b0\\par\\par"
"Built with the mini-mbm game engine.\\par\\par"
"\\b License (MIT)\\b0\\par\\par"
"Copyright (c) mini-mbm contributors\\par\\par"
"Permission is hereby granted, free of charge, to any person obtaining a copy "
"of this software and associated documentation files (the \\ldblquote Software\\rdblquote ), "
"to deal in the Software without restriction, including without limitation the rights "
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies "
"of the Software, and to permit persons to whom the Software is furnished to do so, "
"subject to the following conditions:\\par\\par"
"The above copyright notice and this permission notice shall be included in all "
"copies or substantial portions of the Software.\\par\\par"
"THE SOFTWARE IS PROVIDED \\ldblquote AS IS\\rdblquote , WITHOUT WARRANTY OF ANY KIND, "
"EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS "
"OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN "
"AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH "
"THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\\par}"
)

# ── Write .wxs ────────────────────────────────────────────────────────────────
file(WRITE "${WIX_OUTPUT_WXS}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!-- Generated by mini-mbm make-msi.cmake - do not edit manually. -->
<Wix xmlns=\"http://schemas.microsoft.com/wix/2006/wi\">
    <Product Id=\"*\"
             UpgradeCode=\"{${WIX_UPGRADE_CODE}}\"
             Name=\"${GAME_NAME}\"
             Version=\"1.0.0\"
             Manufacturer=\"mini-mbm\"
             Language=\"1033\">
        <Package InstallerVersion=\"200\" Compressed=\"yes\" InstallScope=\"perUser\" />
        <MajorUpgrade DowngradeErrorMessage=\"A newer version of [ProductName] is already installed.\" />
        <MediaTemplate EmbedCab=\"yes\" />
${_WIX_ICON_XML}
        <Directory Id=\"TARGETDIR\" Name=\"SourceDir\">
            <Directory Id=\"LocalAppDataFolder\">
                <Directory Id=\"ProgramsFolder\" Name=\"Programs\">
                    <Directory Id=\"INSTALLDIR\" Name=\"${GAME_NAME_SAFE}\">
${WXS_BODY}                    </Directory>
                </Directory>
            </Directory>
            <Directory Id=\"ProgramMenuFolder\">
                <Directory Id=\"ProgramMenuDir\" Name=\"${GAME_NAME}\" />
            </Directory>
            <Directory Id=\"DesktopFolder\" />
        </Directory>

${_SHORTCUT_COMP}
        <Feature Id=\"ProductFeature\" Title=\"${GAME_NAME}\" Level=\"1\">
${_COMP_REFS}
        </Feature>

        <WixVariable Id=\"WixUILicenseRtf\" Value=\"${_WIX_LICENSE_RTF}\" />
        <UIRef Id=\"WixUI_Minimal\" />

    </Product>
</Wix>
")
message(STATUS "Generated WiX source: ${WIX_OUTPUT_WXS}")

# ── Build the MSI ─────────────────────────────────────────────────────────────
if(WIX_V4)
    execute_process(
        COMMAND "${WIX_V4}" build "${WIX_OUTPUT_WXS}"
            -o "${WIX_OUTPUT_MSI}"
            -ext WixToolset.UI.wixext
        WORKING_DIRECTORY "${WIN_GAMEDIR}"
        RESULT_VARIABLE _RES)
else()
    set(_WIXOBJ "${OUTPUT_DIR}/${GAME_NAME_SAFE}.wixobj")
    execute_process(
        COMMAND "${WIX_CANDLE}" "${WIX_OUTPUT_WXS}"
            -o "${_WIXOBJ}"
            -ext WixUIExtension
        WORKING_DIRECTORY "${WIN_GAMEDIR}"
        RESULT_VARIABLE _RES1)
    if(_RES1 EQUAL 0)
        execute_process(
            COMMAND "${WIX_LIGHT}" "${_WIXOBJ}"
                -o "${WIX_OUTPUT_MSI}"
                -ext WixUIExtension
                -sice:ICE38
                -sice:ICE57
                -sice:ICE64
                -sw1076
            WORKING_DIRECTORY "${WIN_GAMEDIR}"
            RESULT_VARIABLE _RES)
    else()
        set(_RES ${_RES1})
    endif()
endif()

if(_RES EQUAL 0)
    message(STATUS "--- MSI ready: ${WIX_OUTPUT_MSI}")
else()
    message(FATAL_ERROR "WiX build failed with code ${_RES}")
endif()
