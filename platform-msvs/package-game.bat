@echo off
rem ============================================================================
rem package-game.bat  —  Windows game distribution packager (VS solution path)
rem
rem  Mirrors the CMake -DGAME_ASSETS_DIR workflow for games built with the
rem  Visual Studio solution (platform-msvs/mini-mbm.sln).
rem
rem  Usage:
 rem    package-game.bat "Game Name" "C:\path\to\assets" Debug|Release [icon.ico] [password]
 rem
 rem  Outputs (next to this script, inside GameDir):
 rem    <GameName>.GameDir\           — portable distribution folder
 rem      <GameName>.exe             — renamed engine executable
 rem      distribution.dll           — asset library
 rem      *.dll                      — runtime DLLs
 rem      assets\<GameName>.asset    — encrypted packed assets
rem      launch.bat                 — run shortcut (sets GAME_SAVE_DIR)
rem      <GameName>.ico             — icon (if supplied)
rem
rem  Optional packaging (requires external tools in PATH):
rem    NSIS   → <GameName>-windows-setup.exe
rem    WiX v4 → <GameName>-windows.msi
rem    WiX v3 → <GameName>-windows.msi  (candle + light)
rem ============================================================================
setlocal enabledelayedexpansion

rem ── Arguments ──────────────────────────────────────────────────────────────
set "GAME_NAME=%~1"
set "GAME_ASSETS_DIR=%~2"
set "CONFIG=%~3"
set "GAME_ICON_ICO=%~4"
set "GAME_ASSETS_PASSWORD=%~5"

if "%GAME_NAME%"=="" (
    echo.
    echo  Usage: package-game.bat "Game Name" "C:\path\to\assets" Debug^|Release [icon.ico] [password]
    echo.
    exit /b 1
)
if "%GAME_ASSETS_DIR%"=="" (
    echo.
    echo  Usage: package-game.bat "Game Name" "C:\path\to\assets" Debug^|Release [icon.ico] [password]
    echo.
    exit /b 1
)
if "%CONFIG%"=="" set "CONFIG=Release"

rem ── Derive safe name (spaces → underscores) ─────────────────────────────────
set "GAME_NAME_SAFE=%GAME_NAME: =_%"

rem ── Paths ───────────────────────────────────────────────────────────────────
pushd %~dp0..
set "ENGINE_ROOT=%CD%"
popd

rem VS solution outputs to bin\debug\windows_x86\ or bin\release\windows_x86\
if /i "%CONFIG%"=="Debug" (
    set "BIN_DIR=%ENGINE_ROOT%\bin\debug\windows_x86"
) else (
    set "BIN_DIR=%ENGINE_ROOT%\bin\release\windows_x86"
)

set "GAMEDIR=%ENGINE_ROOT%\%GAME_NAME_SAFE%.GameDir"
set "ASSETS_DST=%GAMEDIR%\assets"
set "EXE_SRC=%BIN_DIR%\mini-mbm.exe"
set "EXE_DST=%GAMEDIR%\%GAME_NAME_SAFE%.exe"

echo.
echo  Game name    : %GAME_NAME%
echo  Config       : %CONFIG%
echo  Engine root  : %ENGINE_ROOT%
echo  Bin dir      : %BIN_DIR%
echo  Assets src   : %GAME_ASSETS_DIR%
echo  GameDir      : %GAMEDIR%
echo.

rem ── Validate inputs ─────────────────────────────────────────────────────────
if not exist "%EXE_SRC%" (
    echo  ERROR: executable not found: %EXE_SRC%
    echo  Build the solution first: msbuild platform-msvs\mini-mbm.sln /p:Configuration=%CONFIG%
    exit /b 1
)
if not exist "%GAME_ASSETS_DIR%\" (
    echo  ERROR: assets directory not found: %GAME_ASSETS_DIR%
    exit /b 1
)

rem ── Create GameDir ──────────────────────────────────────────────────────────
if not exist "%GAMEDIR%"   mkdir "%GAMEDIR%"
if not exist "%ASSETS_DST%" mkdir "%ASSETS_DST%"

rem ── Copy and rename executable ──────────────────────────────────────────────
echo  Copying executable...
copy /y "%EXE_SRC%" "%EXE_DST%" >nul

rem ── Copy runtime DLLs ───────────────────────────────────────────────────────
echo  Copying runtime DLLs...

set "D3D_DLL=%ENGINE_ROOT%\third-party\gles\bin\d3dcompiler_47.dll"
set "EGL_DLL=%ENGINE_ROOT%\third-party\gles\bin\libEGL.dll"
set "GLES_DLL=%ENGINE_ROOT%\third-party\gles\bin\libGLESv2.dll"

rem Copy both DirectX and OpenGL ES DLLs so the package works with either backend
if exist "%D3D_DLL%"      copy /y "%D3D_DLL%"      "%GAMEDIR%\" >nul
if exist "%EGL_DLL%"      copy /y "%EGL_DLL%"      "%GAMEDIR%\" >nul
if exist "%GLES_DLL%"     copy /y "%GLES_DLL%"     "%GAMEDIR%\" >nul

rem ── Copy plugin DLLs from bin output ────────────────────────────────────────
echo  Copying plugin DLLs from %BIN_DIR%...
for %%F in ("%BIN_DIR%\*.dll") do (
    if /i not "%%~nxF"=="d3dcompiler_47.dll" (
        if /i not "%%~nxF"=="libEGL.dll" (
            if /i not "%%~nxF"=="libGLESv2.dll" (
                copy /y "%%F" "%GAMEDIR%\" >nul
            )
        )
    )
)

rem ── Pack game assets with distribution.exe ────────────────────────────────
echo  Packing assets from %GAME_ASSETS_DIR%...
set "DIST_EXE=%BIN_DIR%\distribution.exe"
if not exist "%DIST_EXE%" (
    echo  ERROR: distribution.exe not found: %DIST_EXE%
    echo  Build the 'distribution_exe' project first.
    exit /b 1
)
if not exist "%ASSETS_DST%" mkdir "%ASSETS_DST%"
if "%GAME_ASSETS_PASSWORD%"=="" (
    "%DIST_EXE%" pack "%GAME_ASSETS_DIR%" "%ASSETS_DST%\%GAME_NAME_SAFE%.asset"
) else (
    "%DIST_EXE%" pack "%GAME_ASSETS_DIR%" "%ASSETS_DST%\%GAME_NAME_SAFE%.asset" --password "%GAME_ASSETS_PASSWORD%"
)
if errorlevel 1 (
    echo  ERROR: distribution.exe pack failed.
    exit /b 1
)

rem ── Copy distribution.dll ──────────────────────────────────────────────────
echo  Copying distribution.dll...
copy /y "%BIN_DIR%\distribution.dll" "%GAMEDIR%\" >nul

rem ── Copy icon ───────────────────────────────────────────────────────────────
if not "%GAME_ICON_ICO%"=="" (
    if exist "%GAME_ICON_ICO%" (
        echo  Copying icon...
        copy /y "%GAME_ICON_ICO%" "%GAMEDIR%\%GAME_NAME_SAFE%.ico" >nul
    ) else (
        echo  WARNING: icon not found: %GAME_ICON_ICO%
    )
)

rem ── Write launch.bat ────────────────────────────────────────────────────────
echo  Writing launch.bat...
(
echo @echo off
echo rem Generated by package-game.bat -- do not edit manually.
echo set "GAME_SAVE_DIR=%%APPDATA%%\%GAME_NAME_SAFE%"
echo if not exist "%%GAME_SAVE_DIR%%" mkdir "%%GAME_SAVE_DIR%%"
echo cd /d "%%~dp0"
echo "%GAME_NAME_SAFE%.exe" --name "%GAME_NAME%" --scene "assets\main.lua" %%*
) > "%GAMEDIR%\launch.bat"

echo.
echo  GameDir ready: %GAMEDIR%
echo.

rem ── Write NSIS script ────────────────────────────────────────────────────────
set "NSI_FILE=%ENGINE_ROOT%\%GAME_NAME_SAFE%.nsi"
set "NSI_OUTPUT=%ENGINE_ROOT%\%GAME_NAME_SAFE%-windows-setup.exe"

set "ICON_LINE="
if exist "%GAMEDIR%\%GAME_NAME_SAFE%.ico" (
    set "ICON_LINE=!define MUI_ICON \"%GAMEDIR%\%GAME_NAME_SAFE%.ico\""
)

(
echo !include "MUI2.nsh"
echo.
echo ; Generated by package-game.bat -- do not edit manually.
echo Name "%GAME_NAME%"
echo OutFile "%NSI_OUTPUT%"
echo Unicode True
echo ; Per-user install -- no admin/UAC prompt required
echo InstallDir "$LOCALAPPDATA\Programs\%GAME_NAME_SAFE%"
echo InstallDirRegKey HKCU "Software\%GAME_NAME_SAFE%" ""
echo RequestExecutionLevel user
echo.
echo %ICON_LINE%
echo !define MUI_ABORTWARNING
echo !insertmacro MUI_PAGE_DIRECTORY
echo !insertmacro MUI_PAGE_INSTFILES
echo !insertmacro MUI_UNPAGE_CONFIRM
echo !insertmacro MUI_UNPAGE_INSTFILES
echo !insertmacro MUI_LANGUAGE "English"
echo.
echo Section "Install"
echo     SetOutPath "$INSTDIR"
echo     File /r "%GAMEDIR%\*.*"
echo.
echo     WriteUninstaller "$INSTDIR\uninstall.exe"
echo.
echo     ; Start-menu and desktop shortcuts (per-user shell context)
echo     SetShellVarContext current
echo     CreateDirectory "$SMPROGRAMS\%GAME_NAME%"
echo     CreateShortcut "$SMPROGRAMS\%GAME_NAME%\%GAME_NAME%.lnk" "$INSTDIR\%GAME_NAME_SAFE%.exe" "--name $\"%GAME_NAME%$\" --scene assets\main.lua" "$INSTDIR\%GAME_NAME_SAFE%.ico"
echo     CreateShortcut "$DESKTOP\%GAME_NAME%.lnk" "$INSTDIR\%GAME_NAME_SAFE%.exe" "--name $\"%GAME_NAME%$\" --scene assets\main.lua" "$INSTDIR\%GAME_NAME_SAFE%.ico"
echo.
echo     ; Uninstall registry entry under HKCU (no admin rights needed)
echo     WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\%GAME_NAME_SAFE%" "DisplayName" "%GAME_NAME%"
echo     WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\%GAME_NAME_SAFE%" "UninstallString" "$INSTDIR\uninstall.exe"
echo SectionEnd
echo.
echo Section "Uninstall"
echo     Delete "$INSTDIR\uninstall.exe"
echo     RMDir /r "$INSTDIR"
echo     SetShellVarContext current
echo     Delete "$SMPROGRAMS\%GAME_NAME%\%GAME_NAME%.lnk"
echo     RMDir "$SMPROGRAMS\%GAME_NAME%"
echo     Delete "$DESKTOP\%GAME_NAME%.lnk"
echo     DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\%GAME_NAME_SAFE%"
echo     DeleteRegKey HKCU "Software\%GAME_NAME_SAFE%"
echo SectionEnd
) > "%NSI_FILE%"

rem ── Run NSIS (if available) ──────────────────────────────────────────────────
set "MAKENSIS="
for %%P in (
    "makensis"
    "C:\Program Files (x86)\NSIS\makensis.exe"
    "C:\Program Files\NSIS\makensis.exe"
) do (
    if "!MAKENSIS!"=="" (
        where %%~P >nul 2>&1
        if !errorlevel!==0 set "MAKENSIS=%%~P"
        if exist "%%~P"    set "MAKENSIS=%%~P"
    )
)

if not "!MAKENSIS!"=="" (
    echo  Running NSIS...
    "!MAKENSIS!" "%NSI_FILE%"
    if !errorlevel!==0 (
        echo.
        echo  NSIS installer ready: %NSI_OUTPUT%
        echo.
    ) else (
        echo  WARNING: makensis failed. Check the .nsi script: %NSI_FILE%
    )
) else (
    echo  NSIS not found — skipping installer generation.
    echo  Download: https://nsis.sourceforge.io
)

rem ── Run WiX via standalone make-msi.cmake ───────────────────────────────────
set "MAKE_MSI_SCRIPT=%~dp0make-msi.cmake"
set "MSI_OUTPUT=%ENGINE_ROOT%\%GAME_NAME_SAFE%-windows.msi"

set "MSI_ICON_ARG="
if not "%GAME_ICON_ICO%"=="" if exist "%GAME_ICON_ICO%" (
    set "MSI_ICON_ARG=-DICON_ICO=%GAME_ICON_ICO%"
)

where cmake >nul 2>&1
if %errorlevel%==0 (
    echo  Running WiX MSI generator...
    cmake "-DGAME_NAME=%GAME_NAME%" ^
          "-DGAME_NAME_SAFE=%GAME_NAME_SAFE%" ^
          "-DGAME_DIR=%GAMEDIR%" ^
          "-DOUTPUT_DIR=%ENGINE_ROOT%" ^
          %MSI_ICON_ARG% ^
          -P "%MAKE_MSI_SCRIPT%"
    if !errorlevel!==0 (
        echo.
        echo  MSI installer ready: %MSI_OUTPUT%
        echo.
    ) else (
        echo  WARNING: MSI generation failed. Check output above.
    )
) else (
    echo  cmake not found in PATH — skipping MSI generation.
    echo  Install CMake from https://cmake.org/download/
)

rem ── ZIP portable archive ─────────────────────────────────────────────────────
set "ZIP_OUTPUT=%ENGINE_ROOT%\%GAME_NAME_SAFE%-windows.zip"
echo  Creating ZIP archive...
powershell -NoProfile -Command ^
    "Compress-Archive -Force -Path '%GAMEDIR%' -DestinationPath '%ZIP_OUTPUT%'"
if !errorlevel!==0 (
    echo.
    echo  ZIP archive ready: %ZIP_OUTPUT%
    echo.
) else (
    echo  WARNING: ZIP creation failed.
)

echo.
echo  Done. Distribution folder: %GAMEDIR%
echo.
endlocal
exit /b 0
