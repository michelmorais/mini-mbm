#!/usr/bin/env bash
# copy-game-template.sh — Copy the mini-mbm Lua game template to a new folder.
#
# Usage:
#   ./copy-game-template.sh <destination-folder>
#
# Example:
#   ./copy-game-template.sh /home/michel/my-new-game

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="${SCRIPT_DIR}/game-template"

usage() {
    echo "Usage: $0 <destination-folder>"
    echo ""
    echo "Creates a standalone Lua game in a new or empty folder."
    echo "Includes agent instructions and a snapshot of the current engine docs."
    echo "Engine binaries and plugins are provided externally."
}

if [[ $# -eq 1 && ( "$1" == "--help" || "$1" == "-h" ) ]]; then
    usage
    exit 0
fi
if [[ $# -ne 1 || -z "${1:-}" ]]; then
    usage >&2
    exit 1
fi

# Validate all sources before creating the destination.
for source in game-template/main.lua game-template/AGENTS.md \
              game-template/.github/copilot-instructions.md docs/lua-api.md LICENSE.md; do
    if [[ ! -f "${SCRIPT_DIR}/${source}" ]]; then
        echo "Error: required source not found: ${SCRIPT_DIR}/${source}" >&2
        exit 1
    fi
done

DEST="$1"
if [[ -e "$DEST" && ! -d "$DEST" ]]; then
    echo "Error: destination is not a directory: $DEST" >&2
    exit 1
fi
# Include hidden files and dangling symlinks when checking for existing work.
shopt -s nullglob dotglob
entries=("$DEST"/*)
if [[ ${#entries[@]} -ne 0 ]]; then
    echo "Error: destination must be empty; existing files were not changed: $DEST" >&2
    exit 1
fi

mkdir -p -- "$DEST"
DEST="$(cd -- "$DEST" && pwd)"
mkdir -p -- "$DEST/.github" "$DEST/docs" "$DEST/scenes" "$DEST/concepts"
for asset in sounds sprites tilesets fonts textures meshes; do
    mkdir -p -- "$DEST/assets/$asset"
done

# Copy only the standalone starter, not engine-dependent manual test scenes.
cp -- "$TEMPLATE_DIR/main.lua" "$TEMPLATE_DIR/AGENTS.md" "$DEST/"
cp -- "$TEMPLATE_DIR/.github/copilot-instructions.md" "$DEST/.github/"
ln -s AGENTS.md "$DEST/CLAUDE.md"
# Copy from the canonical source each time, so new API documentation is included
# without maintaining another frozen copy in game-template/.
cp -R -- "$SCRIPT_DIR/docs/." "$DEST/docs/"
cp -- "$SCRIPT_DIR/LICENSE.md" "$DEST/docs/ENGINE-LICENSE.md"

echo "Game template copied to: $DEST"
echo "Agent context: AGENTS.md, CLAUDE.md -> AGENTS.md, .github/copilot-instructions.md"
echo "Current engine documentation: $DEST/docs/lua-api.md"
echo "Run your game with your Lua-enabled engine build:"
printf '  cd %q && /path/to/mini-mbm --scene main.lua\n' "$DEST"
