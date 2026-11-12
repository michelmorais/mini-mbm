@echo off
rem Copies third-party runtime files used by the Visual Studio output.
rem Plugin DLLs are produced directly in OutDir by their own MSBuild projects;
rem they must not be copied from a second/stale directory here.
rem
rem Usage: copy-dlls.bat <OutDir> <x32|Win32|x64>

setlocal EnableExtensions EnableDelayedExpansion

set "DESTINATION_FOLDER=%~1"
set "ARCH=%~2"

if not defined DESTINATION_FOLDER (
    echo [copy-dlls] ERROR: destination folder is missing.
    echo [copy-dlls] Usage: copy-dlls.bat ^<OutDir^> ^<x32^|Win32^|x64^>
    exit /b 1
)

if not defined ARCH (
    echo [copy-dlls] ERROR: architecture is missing.
    echo [copy-dlls] Usage: copy-dlls.bat ^<OutDir^> ^<x32^|Win32^|x64^>
    exit /b 1
)

if /i "%ARCH%"=="x32" set "ARCH=x32"
if /i "%ARCH%"=="Win32" set "ARCH=x32"
if /i "%ARCH%"=="x64" set "ARCH=x64"
if /i not "%ARCH%"=="x32" if /i not "%ARCH%"=="x64" (
    echo [copy-dlls] ERROR: unsupported architecture "%~2".
    echo [copy-dlls] Expected x32, Win32, or x64.
    exit /b 1
)

pushd "%~dp0.." >nul
set "ENGINE_ROOT=%CD%"
popd >nul

if not exist "%DESTINATION_FOLDER%\" (
    mkdir "%DESTINATION_FOLDER%"
    if errorlevel 1 (
        echo [copy-dlls] ERROR: could not create "%DESTINATION_FOLDER%".
        exit /b 1
    )
)

echo [copy-dlls] Copying third-party files to "%DESTINATION_FOLDER%" (%ARCH%)...

call :copy_file "%ENGINE_ROOT%\third-party\gles\bin\libEGL.dll" "%DESTINATION_FOLDER%"
if errorlevel 1 exit /b 1
call :copy_file "%ENGINE_ROOT%\third-party\gles\libs\libEGL.dll.lib" "%DESTINATION_FOLDER%"
if errorlevel 1 exit /b 1
call :copy_file "%ENGINE_ROOT%\third-party\gles\bin\libGLESv2.dll" "%DESTINATION_FOLDER%"
if errorlevel 1 exit /b 1
call :copy_file "%ENGINE_ROOT%\third-party\gles\libs\libGLESv2.dll.lib" "%DESTINATION_FOLDER%"
if errorlevel 1 exit /b 1
call :copy_file "%ENGINE_ROOT%\third-party\gles\bin\d3dcompiler_47.dll" "%DESTINATION_FOLDER%"
if errorlevel 1 exit /b 1
call :copy_file "%ENGINE_ROOT%\src\mini-mbm-lib\mini-mbm-lib.h" "%DESTINATION_FOLDER%"
if errorlevel 1 exit /b 1

if /i "%ARCH%"=="x32" (
    call :copy_file "%ENGINE_ROOT%\third-party\portaudio\Windows\bin\portaudio_x86.dll" "%DESTINATION_FOLDER%"
    if errorlevel 1 exit /b 1
)

call :copy_tree "%ENGINE_ROOT%\editor" "%DESTINATION_FOLDER%\editor"
if errorlevel 1 exit /b 1

echo [copy-dlls] Done. Plugin DLLs, including interaction_animation_runtime.dll,
echo [copy-dlls] are supplied by their MSBuild targets in the same output folder.
exit /b 0

:copy_file
if not exist "%~1" (
    echo [copy-dlls] ERROR: required file not found: "%~1"
    exit /b 1
)
copy /Y "%~1" "%~2\" >nul
if errorlevel 1 (
    echo [copy-dlls] ERROR: failed to copy "%~1".
    exit /b 1
)
exit /b 0

:copy_tree
if not exist "%~1\" (
    echo [copy-dlls] ERROR: required directory not found: "%~1"
    exit /b 1
)
xcopy "%~1" "%~2\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [copy-dlls] ERROR: failed to copy "%~1".
    exit /b 1
)
exit /b 0
