#!/usr/bin/env bash
# =============================================================================
# upload-to-steam.sh  —  Generate SteamPipe VDF scripts and upload to Steam
#
#  Usage:
#    ./upload-to-steam.sh "Game Name" <AppID> <DepotID> "/path/to/GameDir"
#                         "/path/to/steamcmd.sh" [Description] [Branch]
#
#  Example:
#    ./upload-to-steam.sh "Tower Defense Monster" 1888760 1888761 \
#        "/home/user/mini-mbm/build/Tower_Defense_Monster.GameDir" \
#        "$HOME/.steam/steamcmd/steamcmd.sh" \
#        "v1.2.0 release" "beta"
#
#  Parameters:
#    $1  Game name          (any string, used for display only)
#    $2  Steam App ID       (numeric, e.g. 1888760)
#    $3  Steam Depot ID     (numeric, usually App ID + 1, e.g. 1888761)
#    $4  GameDir path       (output folder produced by the CMake build or package step)
#    $5  steamcmd path      (full path to steamcmd.sh or steamcmd binary)
#    $6  Build description  (optional, shown in Steamworks dashboard)
#    $7  Branch             (optional, "" = not set live; "public" goes live immediately)
#
#  What it does:
#    1. Creates a steamscripts/ subfolder inside the GameDir.
#    2. Generates depot_build_<DepotID>.vdf  — maps GameDir content to the depot.
#    3. Generates app_build_<AppID>.vdf      — describes the app build.
#    4. Runs steamcmd to upload the build.
#    5. Prints the SteamPipe build log path on success.
#
#  SteamCMD installation:
#    Ubuntu/Debian:  sudo apt-get install steamcmd
#    Manual:         mkdir ~/steamcmd && cd ~/steamcmd
#                    curl -sqL "https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz" | tar zxvf -
#    macOS:          brew install steamcmd
#                    or manually extract steamcmd_osx.tar.gz
#    More info:      https://developer.valvesoftware.com/wiki/SteamCMD
# =============================================================================
set -euo pipefail

# ── Arguments ─────────────────────────────────────────────────────────────────
GAME_NAME="${1:-}"
APP_ID="${2:-}"
DEPOT_ID="${3:-}"
GAME_DIR="${4:-}"
STEAMCMD="${5:-}"
BUILD_DESC="${6:-Built by upload-to-steam.sh}"
BRANCH="${7:-}"

# ── Usage ─────────────────────────────────────────────────────────────────────
usage() {
    cat <<EOF

Usage:
  ./upload-to-steam.sh "Game Name" AppID DepotID "/path/to/GameDir"
                       "/path/to/steamcmd.sh" [Description] [Branch]

Example:
  ./upload-to-steam.sh "Tower Defense Monster" 1888760 1888761 \\
      "/home/user/mini-mbm/build/Tower_Defense_Monster.GameDir" \\
      "\$HOME/.steam/steamcmd/steamcmd.sh" \\
      "v1.2.0 release" "beta"

Parameters:
  Game Name    - any display name for the game
  AppID        - your Steam App ID (from Steamworks partner dashboard)
  DepotID      - your Steam Depot ID (usually AppID + 1)
  GameDir      - output folder produced by the CMake build or package step
  steamcmd     - full path to steamcmd.sh or steamcmd binary
  Description  - (optional) build description shown in the dashboard
  Branch       - (optional) branch to set live: "beta", "public", etc.
                 Leave empty to upload without going live.

EOF
    exit 1
}

# ── Validate required arguments ───────────────────────────────────────────────
[[ -z "$GAME_NAME" || -z "$APP_ID" || -z "$DEPOT_ID" || -z "$GAME_DIR" || -z "$STEAMCMD" ]] && usage

# ── Validate numeric App / Depot IDs ─────────────────────────────────────────
if ! [[ "$APP_ID" =~ ^[0-9]+$ ]]; then
    echo " ERROR: App ID must be numeric: $APP_ID"
    exit 1
fi
if ! [[ "$DEPOT_ID" =~ ^[0-9]+$ ]]; then
    echo " ERROR: Depot ID must be numeric: $DEPOT_ID"
    exit 1
fi

# ── Validate paths ────────────────────────────────────────────────────────────
if [[ ! -d "$GAME_DIR" ]]; then
    echo " ERROR: GameDir not found: $GAME_DIR"
    echo " Run the CMake build (or package step) first to produce the distribution folder."
    exit 1
fi
if [[ ! -f "$STEAMCMD" ]]; then
    echo " ERROR: steamcmd not found: $STEAMCMD"
    echo " Install SteamCMD from: https://developer.valvesoftware.com/wiki/SteamCMD"
    exit 1
fi

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPTS_DIR="$GAME_DIR/steamscripts"
BUILD_OUTPUT="$SCRIPTS_DIR/output"
DEPOT_VDF="$SCRIPTS_DIR/depot_build_${DEPOT_ID}.vdf"
APP_VDF="$SCRIPTS_DIR/app_build_${APP_ID}.vdf"

echo
echo " Game name    : $GAME_NAME"
echo " App ID       : $APP_ID"
echo " Depot ID     : $DEPOT_ID"
echo " GameDir      : $GAME_DIR"
echo " steamcmd     : $STEAMCMD"
echo " Description  : $BUILD_DESC"
if [[ -n "$BRANCH" ]]; then
    echo " Branch       : $BRANCH"
else
    echo " Branch       : (not set live)"
fi
echo

# ── Create scripts directory ──────────────────────────────────────────────────
mkdir -p "$SCRIPTS_DIR" "$BUILD_OUTPUT"

# ── Generate depot_build_<DepotID>.vdf ───────────────────────────────────────
echo " Generating $DEPOT_VDF..."
cat > "$DEPOT_VDF" <<EOF
"DepotBuildConfig"
{
    "DepotID"       "$DEPOT_ID"
    "ContentRoot"   "$GAME_DIR"
    "FileMapping"
    {
        "LocalPath"     "*"
        "DepotPath"     "."
        "recursive"     "1"
    }
    // Exclude debug symbols, SteamPipe scripts, and the dev-only appID file
    "FileExclusion"     "*.pdb"
    "FileExclusion"     "steamscripts/*"
    "FileExclusion"     "steam_appid.txt"
}
EOF

# ── Generate app_build_<AppID>.vdf ───────────────────────────────────────────
echo " Generating $APP_VDF..."
cat > "$APP_VDF" <<EOF
"appbuild"
{
    "appid"         "$APP_ID"
    "desc"          "$BUILD_DESC"
    "buildoutput"   "$BUILD_OUTPUT"
    "contentroot"   ""
    "setlive"       "$BRANCH"
    "preview"       "0"
    "depots"
    {
        "$DEPOT_ID"     "depot_build_${DEPOT_ID}.vdf"
    }
}
EOF

echo " VDF scripts written to: $SCRIPTS_DIR"
echo

# ── Prompt for Steam login ────────────────────────────────────────────────────
echo " Enter your Steam account username (the one with Steamworks access):"
read -rp "  Username: " STEAM_USER
if [[ -z "$STEAM_USER" ]]; then
    echo " ERROR: Steam username is required."
    exit 1
fi

echo
echo " Running steamcmd upload..."
echo " (You may be prompted for your password and Steam Guard code)"
echo

# ── Run steamcmd ──────────────────────────────────────────────────────────────
"$STEAMCMD" \
    +login "$STEAM_USER" \
    +run_app_build "$APP_VDF" \
    +quit

if [[ $? -ne 0 ]]; then
    echo
    echo " ERROR: steamcmd exited with an error. Check the output above."
    echo " Build logs: $BUILD_OUTPUT"
    exit 1
fi

echo
echo " Upload complete."
echo " Build logs : $BUILD_OUTPUT"
echo " VDF scripts: $SCRIPTS_DIR"
echo

# ── Remind about promoting to public ─────────────────────────────────────────
if [[ -z "$BRANCH" ]]; then
    echo " NOTE: No branch was specified. The build was uploaded but is NOT set live."
    echo " To make it available to players, go to:"
    echo "   https://partner.steamgames.com/apps/builds/$APP_ID"
    echo " and click \"Set Build Live\" for the desired branch."
    echo
elif [[ "${BRANCH,,}" == "public" ]]; then
    echo " Build is set live on the PUBLIC branch and is now available to all players."
    echo
else
    echo " Build is set live on the '$BRANCH' branch."
    echo " To promote to public: https://partner.steamgames.com/apps/builds/$APP_ID"
    echo
fi
