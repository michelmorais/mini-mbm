@echo off
rem ============================================================================
rem upload-to-steam.bat  —  Generate SteamPipe VDF scripts and upload to Steam
rem
rem  Usage:
rem    upload-to-steam.bat "Game Name" <AppID> <DepotID> "C:\path\to\GameDir"
rem                        "C:\path\to\steamcmd.exe" [Description] [Branch]
rem
rem  Example:
rem    upload-to-steam.bat "Tower Defense Monster" 680230 680231 ^
rem        "C:\...\Tower_Defense_Monster.GameDir" ^
rem        "C:\steamcmd\steamcmd.exe" ^
rem        "v1.2.0 release" "beta"
rem
rem  Parameters:
rem    %1  Game name          (any string, used for display only)
rem    %2  Steam App ID       (numeric, e.g. 680230)
rem    %3  Steam Depot ID     (numeric, usually App ID + 1, e.g. 680231)
rem    %4  GameDir path       (output folder produced by package-game.bat)
rem    %5  steamcmd.exe path  (full path to steamcmd.exe)
rem    %6  Build description  (optional, shown in Steamworks dashboard)
rem    %7  Branch             (optional, default: "" = no live branch; "public" goes live)
rem
rem  What it does:
rem    1. Creates a steamscripts\ subfolder inside the GameDir.
rem    2. Generates depot_build_<DepotID>.vdf  — maps GameDir content to the depot.
rem    3. Generates app_build_<AppID>.vdf      — describes the app build.
rem    4. Runs steamcmd to upload the build.
rem    5. Prints the SteamPipe build log path on success.
rem
rem  Notes:
rem    - The Steam client does NOT need to be running during upload.
rem    - steamcmd will prompt for Steam credentials on the first run; subsequent
rem      runs use the cached login (guard code may still be required).
rem    - SteamCMD download: https://developer.valvesoftware.com/wiki/SteamCMD
rem    - Steamworks partner portal: https://partner.steamgames.com/
rem ============================================================================
setlocal enabledelayedexpansion

rem ── Arguments ──────────────────────────────────────────────────────────────
set "GAME_NAME=%~1"
set "APP_ID=%~2"
set "DEPOT_ID=%~3"
set "GAME_DIR=%~4"
set "STEAMCMD=%~5"
set "BUILD_DESC=%~6"
set "BRANCH=%~7"

rem ── Validate required arguments ────────────────────────────────────────────
if "%GAME_NAME%"=="" goto :usage
if "%APP_ID%"==""   goto :usage
if "%DEPOT_ID%"=="" goto :usage
if "%GAME_DIR%"=="" goto :usage
if "%STEAMCMD%"=="" goto :usage

rem ── Validate numeric App / Depot IDs ───────────────────────────────────────
set "ID_CHECK=%APP_ID%"
for /f "delims=0123456789" %%C in ("%ID_CHECK%") do (
    echo  ERROR: App ID must be numeric: %APP_ID%
    exit /b 1
)
set "ID_CHECK=%DEPOT_ID%"
for /f "delims=0123456789" %%C in ("%ID_CHECK%") do (
    echo  ERROR: Depot ID must be numeric: %DEPOT_ID%
    exit /b 1
)

rem ── Validate paths ─────────────────────────────────────────────────────────
if not exist "%GAME_DIR%\" (
    echo  ERROR: GameDir not found: %GAME_DIR%
    echo  Run package-game.bat first to produce the distribution folder.
    exit /b 1
)
if not exist "%STEAMCMD%" (
    echo  ERROR: steamcmd.exe not found: %STEAMCMD%
    echo  Download SteamCMD from: https://developer.valvesoftware.com/wiki/SteamCMD
    exit /b 1
)

rem ── Defaults ───────────────────────────────────────────────────────────────
if "%BUILD_DESC%"=="" set "BUILD_DESC=Built by upload-to-steam.bat"

rem ── Derive safe name ───────────────────────────────────────────────────────
set "GAME_NAME_SAFE=%GAME_NAME: =_%"

rem ── Paths ──────────────────────────────────────────────────────────────────
set "SCRIPTS_DIR=%GAME_DIR%\steamscripts"
set "BUILD_OUTPUT=%GAME_DIR%\steamscripts\output"
set "DEPOT_VDF=%SCRIPTS_DIR%\depot_build_%DEPOT_ID%.vdf"
set "APP_VDF=%SCRIPTS_DIR%\app_build_%APP_ID%.vdf"

echo.
echo  Game name    : %GAME_NAME%
echo  App ID       : %APP_ID%
echo  Depot ID     : %DEPOT_ID%
echo  GameDir      : %GAME_DIR%
echo  steamcmd     : %STEAMCMD%
echo  Description  : %BUILD_DESC%
if not "%BRANCH%"=="" (
    echo  Branch       : %BRANCH%
) else (
    echo  Branch       : ^(not set live^)
)
echo.

rem ── Create scripts directory ───────────────────────────────────────────────
if not exist "%SCRIPTS_DIR%"    mkdir "%SCRIPTS_DIR%"
if not exist "%BUILD_OUTPUT%"   mkdir "%BUILD_OUTPUT%"

rem ── Generate depot_build_<DepotID>.vdf ────────────────────────────────────
echo  Generating %DEPOT_VDF%...
(
echo "DepotBuildConfig"
echo {
echo     "DepotID"   "%DEPOT_ID%"
echo     "ContentRoot"   "%GAME_DIR%"
echo     "FileMapping"
echo     {
echo         "LocalPath"     "*"
echo         "DepotPath"     "."
echo         "recursive"     "1"
echo     }
echo     // Exclude debug symbols and SteamPipe scripts themselves
echo     "FileExclusion"     "*.pdb"
echo     "FileExclusion"     "steamscripts\*"
echo }
) > "%DEPOT_VDF%"

rem ── Generate app_build_<AppID>.vdf ────────────────────────────────────────
echo  Generating %APP_VDF%...

set "SETLIVE_VALUE="
if not "%BRANCH%"=="" set "SETLIVE_VALUE=%BRANCH%"

(
echo "appbuild"
echo {
echo     "appid"         "%APP_ID%"
echo     "desc"          "%BUILD_DESC%"
echo     "buildoutput"   "%BUILD_OUTPUT%"
echo     "contentroot"   ""
echo     "setlive"       "%SETLIVE_VALUE%"
echo     "preview"       "0"
echo     "depots"
echo     {
echo         "%DEPOT_ID%"    "depot_build_%DEPOT_ID%.vdf"
echo     }
echo }
) > "%APP_VDF%"

echo  VDF scripts written to: %SCRIPTS_DIR%
echo.

rem ── Prompt for Steam login ─────────────────────────────────────────────────
echo  Enter your Steam account username (the one with Steamworks access):
set /p "STEAM_USER=  Username: "
if "%STEAM_USER%"=="" (
    echo  ERROR: Steam username is required.
    exit /b 1
)

echo.
echo  Running steamcmd upload...
echo  (You may be prompted for your password and Steam Guard code)
echo.

rem ── Run steamcmd ──────────────────────────────────────────────────────────
"%STEAMCMD%" ^
    +login "%STEAM_USER%" ^
    +run_app_build "%APP_VDF%" ^
    +quit

if errorlevel 1 (
    echo.
    echo  ERROR: steamcmd exited with an error. Check the output above.
    echo  Build logs: %BUILD_OUTPUT%
    exit /b 1
)

echo.
echo  Upload complete.
echo  Build logs : %BUILD_OUTPUT%
echo  VDF scripts: %SCRIPTS_DIR%
echo.

rem ── Remind about promoting to public ──────────────────────────────────────
if "%BRANCH%"=="" (
    echo  NOTE: No branch was specified. The build was uploaded but is NOT set live.
    echo  To make it available to players, go to:
    echo    https://partner.steamgames.com/apps/builds/%APP_ID%
    echo  and click "Set Build Live" for the desired branch.
    echo.
) else if /i "%BRANCH%"=="public" (
    echo  Build is set live on the PUBLIC branch and is now available to all players.
    echo.
) else (
    echo  Build is set live on the '%BRANCH%' branch.
    echo  To promote to public: https://partner.steamgames.com/apps/builds/%APP_ID%
    echo.
)

endlocal
exit /b 0

rem ── Usage ──────────────────────────────────────────────────────────────────
:usage
echo.
echo  Usage:
echo    upload-to-steam.bat "Game Name" AppID DepotID "C:\path\to\GameDir"
echo                        "C:\path\to\steamcmd.exe" [Description] [Branch]
echo.
echo  Example:
echo    upload-to-steam.bat "Tower Defense Monster" 680230 680231 ^
echo        "C:\...\Tower_Defense_Monster.GameDir" ^
echo        "C:\steamcmd\steamcmd.exe" ^
echo        "v1.2.0 release" "beta"
echo.
echo  Parameters:
echo    Game Name    - any display name for the game
echo    AppID        - your Steam App ID (from Steamworks partner dashboard)
echo    DepotID      - your Steam Depot ID (usually AppID + 1)
echo    GameDir      - output folder produced by package-game.bat
echo    steamcmd.exe - full path to steamcmd.exe
echo    Description  - (optional) build description shown in the dashboard
echo    Branch       - (optional) branch to set live: "beta", "public", etc.
echo                   Leave empty to upload without going live.
echo.
endlocal
exit /b 1
