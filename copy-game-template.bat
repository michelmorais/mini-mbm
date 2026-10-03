@echo off
rem +--------------------------------------------------------------------------+
rem | MIT License - Copyright (c) Michel Braz de Morais                         |
rem | See LICENSE.md for the full license text.                                |
rem +--------------------------------------------------------------------------+
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" goto usage_error
if not "%~2"=="" goto usage_error
if /i "%~1"=="--help" goto help
if /i "%~1"=="-h" goto help
if /i "%~1"=="/?" goto help

set "ENGINE_DIR=%~dp0"
set "DEST=%~f1"

rem Validate sources before creating the destination.
for %%F in ("game-template\main.lua" "game-template\AGENTS.md" "game-template\.github\copilot-instructions.md" "docs\lua-api.md" "LICENSE.md") do (
    if not exist "%ENGINE_DIR%%%~F" (
        echo Error: required source not found: "%ENGINE_DIR%%%~F" 1>&2
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
    echo Error: destination must be empty; existing files were not changed: "%DEST%" 1>&2
    exit /b 1
)

:copy_template
for %%D in (.github docs scenes concepts assets\sounds assets\sprites assets\tilesets assets\fonts assets\textures assets\meshes) do (
    mkdir "%DEST%\%%D"
    if errorlevel 1 goto failed
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
mklink CLAUDE.md AGENTS.md >nul 2>&1
if not errorlevel 1 goto link_ready
mklink /H CLAUDE.md AGENTS.md >nul 2>&1
if errorlevel 1 goto link_failed
echo Note: CLAUDE.md uses a hard link. Replacing either file can break synchronization.
:link_ready
popd

rem Snapshot the canonical docs each time, including newly added references.
xcopy "%ENGINE_DIR%docs\*" "%DEST%\docs\" /E /H /I /Y >nul
if errorlevel 1 goto failed
copy /y "%ENGINE_DIR%LICENSE.md" "%DEST%\docs\ENGINE-LICENSE.md" >nul
if errorlevel 1 goto failed

echo Game template copied to: "%DEST%"
echo Agent context: AGENTS.md, CLAUDE.md, .github\copilot-instructions.md
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
echo Usage: %~nx0 ^<destination-folder^>
echo.
echo Creates a standalone Lua game in a new or empty folder.
echo Includes agent instructions and a snapshot of the current engine docs.
echo Engine binaries and plugins are provided externally.
exit /b 0
