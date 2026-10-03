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
    echo "Creates a standalone Lua game; asks before overwriting a nonempty folder."
    echo "Includes agent instructions and a selection of current game-development docs."
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
              game-template/.github/copilot-instructions.md game-template/docs-list.txt LICENSE.md; do
    if [[ ! -f "${SCRIPT_DIR}/${source}" ]]; then
        echo "Error: required source not found: ${SCRIPT_DIR}/${source}" >&2
        exit 1
    fi
done

docs=()
while IFS= read -r doc || [[ -n "$doc" ]]; do
    doc="${doc%$'\r'}"
    [[ -z "$doc" ]] && continue
    if [[ ! -f "$SCRIPT_DIR/docs/$doc" ]]; then
        echo "Error: required document not found: $doc" >&2
        exit 1
    fi
    docs+=("$doc")
done < "$TEMPLATE_DIR/docs-list.txt"

DEST="$1"
if [[ -e "$DEST" && ! -d "$DEST" ]]; then
    echo "Error: destination is not a directory: $DEST" >&2
    exit 1
fi
# Refuse directory/file collisions before changing any project files.
for target in main.lua AGENTS.md CLAUDE.md .github/copilot-instructions.md docs/ENGINE-LICENSE.md "${docs[@]/#/docs/}"; do
    if [[ -d "$DEST/$target" ]]; then
        echo "Error: expected a file, found a directory: $DEST/$target" >&2
        exit 1
    fi
done
# Include hidden files and dangling symlinks when checking for existing work.
shopt -s nullglob dotglob
entries=("$DEST"/*)
if [[ ${#entries[@]} -ne 0 ]]; then
    echo "Destination contains files: $DEST"
    echo "This overwrites main.lua, agent instructions, CLAUDE.md and selected docs."
    echo "Other files are preserved."
    answer=""
    if ! read -r -p "Continue? [y/N] " answer || [[ ! "$answer" =~ ^([yY]|[yY][eE][sS])$ ]]; then
        echo "Cancelled; existing files were not changed."
        exit 1
    fi
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
if [[ -e "$DEST/CLAUDE.md" || -L "$DEST/CLAUDE.md" ]]; then
    rm -- "$DEST/CLAUDE.md"
fi
ln -s AGENTS.md "$DEST/CLAUDE.md"
# Both launchers use the same curated list; do not copy internal plans/audits.
for doc in "${docs[@]}"; do
    cp -- "$SCRIPT_DIR/docs/$doc" "$DEST/docs/$doc"
done
cp -- "$SCRIPT_DIR/LICENSE.md" "$DEST/docs/ENGINE-LICENSE.md"

echo "Game template copied to: $DEST"
echo "Agent context: AGENTS.md, CLAUDE.md -> AGENTS.md, .github/copilot-instructions.md"
echo "Current engine documentation: $DEST/docs/lua-api.md"
echo "Run your game with your Lua-enabled engine build:"
printf '  cd %q && /path/to/mini-mbm --scene main.lua\n' "$DEST"
