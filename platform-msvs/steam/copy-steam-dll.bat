@echo off
rem copy-steam-dll.bat
rem Copies the correct Steam runtime DLL from the Steamworks SDK redistributable_bin
rem to the Visual Studio output directory.
rem
rem Usage (called automatically by the steam.vcxproj PostBuildEvent):
rem   copy-steam-dll.bat <OutDir> <Platform>
rem       <OutDir>    -- $(OutDir) from MSBuild, e.g. C:\...\platform-msvs\Debug\
rem       <Platform>  -- "x64" or "Win32"

setlocal enabledelayedexpansion

set "OUT=%~1"
set "PLATFORM=%~2"

if not defined STEAMWORKS_SDK_PATH (
    echo [steam] WARNING: STEAMWORKS_SDK_PATH is not set. Skipping DLL copy.
    exit /b 0
)

if /i "%PLATFORM%"=="x64" (
    set "STEAM_DLL=%STEAMWORKS_SDK_PATH%\redistributable_bin\win64\steam_api64.dll"
    set "STEAM_DLL_NAME=steam_api64.dll"
) else (
    set "STEAM_DLL=%STEAMWORKS_SDK_PATH%\redistributable_bin\steam_api.dll"
    set "STEAM_DLL_NAME=steam_api.dll"
)

if not exist "!STEAM_DLL!" (
    echo [steam] ERROR: DLL not found at "!STEAM_DLL!"
    echo [steam] Check that STEAMWORKS_SDK_PATH points to the SDK root.
    exit /b 1
)

echo [steam] Copying "!STEAM_DLL!" -^> "!OUT!!STEAM_DLL_NAME!"
copy /Y "!STEAM_DLL!" "!OUT!!STEAM_DLL_NAME!"
if errorlevel 1 (
    echo [steam] ERROR: Copy failed.
    exit /b 1
)

echo [steam] Done.
endlocal
