@echo off
rem +--------------------------------------------------------------------------+
rem | MIT License - Copyright (c) Michel Braz de Morais                         |
rem | See LICENSE.md for the full license text.                                |
rem +--------------------------------------------------------------------------+
setlocal EnableExtensions DisableDelayedExpansion

set "ENGINE_DIR=%~dp0"
set "PROFILE=2d"
set "DEST="

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--help" goto help
if /i "%~1"=="-h" goto help
if /i "%~1"=="/?" goto help
if /i "%~1"=="--profile" (
    if "%~2"=="" goto usage_error
    set "PROFILE=%~2"
    shift
    shift
    goto parse_args
)
set "ARG=%~1"
if /i "%ARG:~0,10%"=="--profile=" (
    set "PROFILE=%ARG:~10%"
    if not defined PROFILE goto usage_error
    shift
    goto parse_args
)
if "%ARG:~0,1%"=="-" goto usage_error
if defined DEST goto usage_error
set "DEST=%~f1"
shift
goto parse_args

:args_done
if not defined DEST goto usage_error

set "DOC_LIST=docs-list-%PROFILE%.txt"
if /i "%PROFILE%"=="2d" set "DOC_LIST=docs-list.txt"
if /i "%PROFILE%"=="2d5" goto profile_valid
if /i "%PROFILE%"=="3d" goto profile_valid
if /i "%PROFILE%"=="animation" goto profile_valid
if /i "%PROFILE%"=="authoring" goto profile_valid
if /i "%PROFILE%"=="all" goto profile_valid
echo Error: unknown profile: "%PROFILE%" 1>&2
goto usage_error

:profile_valid

rem Validate sources before creating the destination.
for %%F in ("game-template\main.lua" "game-template\AGENTS.md" "game-template\.github\copilot-instructions.md" "game-template\%DOC_LIST%" "LICENSE.md") do (
    if not exist "%ENGINE_DIR%%%~F" (
        echo Error: required source not found: "%ENGINE_DIR%%%~F" 1>&2
        exit /b 1
    )
)

for /f "usebackq eol=# delims=" %%F in ("%ENGINE_DIR%game-template\%DOC_LIST%") do (
    if not exist "%ENGINE_DIR%docs\%%F" (
        echo Error: required document not found: "%%F" 1>&2
        exit /b 1
    )
)

rem Refuse directory/file collisions before copying or deleting link entries.
for %%F in (main.lua AGENTS.md CLAUDE.md .github\copilot-instructions.md docs\ENGINE-LICENSE.md) do (
    if exist "%DEST%\%%F\" (
        echo Error: expected a file, found a directory: "%DEST%\%%F" 1>&2
        exit /b 1
    )
)
for /f "usebackq eol=# delims=" %%F in ("%ENGINE_DIR%game-template\%DOC_LIST%") do (
    if exist "%DEST%\docs\%%F\" (
        echo Error: expected a file, found a directory: "%DEST%\docs\%%F" 1>&2
        exit /b 1
    )
)

if exist "%DEST%\" goto check_empty
if exist "%DEST%" (
    echo Error: destination is not a directory: "%DEST%" 1>&2
    exit /b 1
)
mkdir "%DEST%"
if errorlevel 1 goto failed
goto copy_template

:check_empty
rem /a includes hidden and system entries; names are never executed.
for /f "eol=| delims=" %%F in ('dir /a /b "%DEST%" 2^>nul') do (
    goto confirm_overwrite
)

goto copy_template

:confirm_overwrite
echo Destination contains files: "%DEST%"
echo This overwrites main.lua, agent instructions, CLAUDE.md and selected docs.
echo Other files are preserved.
choice /c YN /n /m "Continue? [Y/N] "
if errorlevel 2 goto cancelled
if errorlevel 1 goto copy_template
goto cancelled

:cancelled
echo Cancelled; existing files were not changed.
exit /b 1

:copy_template
for %%D in (.github docs scenes concepts assets\sounds assets\sprites assets\tilesets assets\fonts assets\textures assets\meshes) do (
    if not exist "%DEST%\%%D\" (
        mkdir "%DEST%\%%D"
        if errorlevel 1 goto failed
    )
)

rem Exclude engine-dependent manual test scenes.
copy /y "%ENGINE_DIR%game-template\main.lua" "%DEST%\main.lua" >nul
if errorlevel 1 goto failed
copy /y "%ENGINE_DIR%game-template\AGENTS.md" "%DEST%\AGENTS.md" >nul
if errorlevel 1 goto failed
copy /y "%ENGINE_DIR%game-template\.github\copilot-instructions.md" "%DEST%\.github\copilot-instructions.md" >nul
if errorlevel 1 goto failed

rem A relative symlink keeps the project movable. A hard link also shares content
rem and can be created on NTFS without the symbolic-link privilege.
pushd "%DEST%"
if errorlevel 1 goto failed
rem DEL removes the old link or file without deleting its target.
rem Also attempt deletion for a dangling symbolic link.
del /q /a CLAUDE.md >nul 2>&1
mklink CLAUDE.md AGENTS.md >nul 2>&1
if not errorlevel 1 goto link_ready
mklink /H CLAUDE.md AGENTS.md >nul 2>&1
if errorlevel 1 goto link_failed
echo Note: CLAUDE.md uses a hard link. Replacing either file can break synchronization.
:link_ready
popd

rem Both launchers share the selected game-development documentation list.
for /f "usebackq eol=# delims=" %%F in ("%ENGINE_DIR%game-template\%DOC_LIST%") do (
    copy /y "%ENGINE_DIR%docs\%%F" "%DEST%\docs\%%F" >nul
    if errorlevel 1 goto failed
)
copy /y "%ENGINE_DIR%LICENSE.md" "%DEST%\docs\ENGINE-LICENSE.md" >nul
if errorlevel 1 goto failed

echo Game template copied to: "%DEST%"
echo Agent context: AGENTS.md, CLAUDE.md, .github\copilot-instructions.md
echo Documentation profile: %PROFILE%
echo Current engine documentation: "%DEST%\docs\lua-api.md"
echo Run your game with your Lua-enabled engine build:
echo   cd /d "%DEST%"
echo   "C:\path\to\mini_mbm.exe" --scene main.lua
exit /b 0

:link_failed
popd
echo Error: could not link CLAUDE.md to AGENTS.md. Use a filesystem supporting links. 1>&2
goto failed

:failed
echo Error: template creation failed in "%DEST%". Partial files may remain. 1>&2
exit /b 1

:usage_error
call :usage
exit /b 1

:help
call :usage
exit /b 0

:usage
echo Usage: %~nx0 [--profile ^<2d^|2d5^|3d^|animation^|authoring^|all^>] ^<destination-folder^>
echo.
echo Creates a standalone Lua game; asks before overwriting a nonempty folder.
echo The default profile is 2d. Profiles control which engine docs are copied.
echo Profiles: 2d, 2d5, 3d, animation, authoring, all.
echo Engine binaries and plugins are provided externally.
exit /b 0
