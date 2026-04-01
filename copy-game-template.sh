#!/usr/bin/env bash
# copy-game-template.sh — Copy the mini-mbm Lua game template to a new folder.
#
# Usage:
#   ./copy-game-template.sh <destination-folder>
#
# Example:
#   ./copy-game-template.sh /home/michel/my-new-game

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="${SCRIPT_DIR}/game-template"

if [[ -z "$1" ]]; then
    echo "Usage: $0 <destination-folder>"
    echo ""
    echo "Copies the mini-mbm Lua game template to the specified folder."
    echo "The destination folder will be created if it does not exist."
    echo ""
    echo "Example:"
    echo "  $0 /home/michel/my-new-game"
    exit 1
fi

DEST="$1"

if [[ ! -d "${TEMPLATE_DIR}" ]]; then
    echo "Error: template directory not found: ${TEMPLATE_DIR}" >&2
    exit 1
fi

mkdir -p "${DEST}"
cp -r "${TEMPLATE_DIR}/." "${DEST}/"

echo "Game template copied to: ${DEST}"
echo "Run your game with:"
echo "  cd \"${DEST}\" && /path/to/mini-mbm main.lua"
