#!/usr/bin/env bash
# copy-game-template.sh — Copy the mini-mbm Lua game template to a new folder.
#
# Usage:
#   ./copy-game-template.sh [--profile <name>] <destination-folder>
#
# Example:
#   ./copy-game-template.sh --profile 2d5 /home/michel/my-new-game

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="${SCRIPT_DIR}/game-template"

usage() {
    echo "Usage: $0 [--profile <2d|2d5|3d|animation|authoring|all>] <destination-folder>"
    echo ""
    echo "Creates a standalone Lua game; asks before overwriting a nonempty folder."
    echo "The default profile is 2d. Profiles control which engine docs are copied."
    echo "Profiles: 2d, 2d5, 3d, animation, authoring, all."
    echo "Engine binaries and plugins are provided externally."
}

PROFILE="2d"
DEST=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --profile)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "Error: --profile requires a profile name." >&2
                usage >&2
                exit 1
            fi
            PROFILE="$2"
            shift 2
            ;;
        --profile=*)
            PROFILE="${1#--profile=}"
            if [[ -z "$PROFILE" ]]; then
                echo "Error: --profile requires a profile name." >&2
                usage >&2
                exit 1
            fi
            shift
            ;;
        --*)
            echo "Error: unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            if [[ -n "$DEST" ]]; then
                usage >&2
                exit 1
            fi
            DEST="$1"
            shift
            ;;
    esac
done

if [[ -z "$DEST" ]]; then
    usage >&2
    exit 1
fi

case "$PROFILE" in
    2d)
        DOC_LIST="docs-list.txt"
        ;;
    2d5|3d|animation|authoring|all)
        DOC_LIST="docs-list-${PROFILE}.txt"
        ;;
    *)
        echo "Error: unknown profile: $PROFILE" >&2
        usage >&2
        exit 1
        ;;
esac

# Validate all sources before creating the destination.
for source in game-template/main.lua game-template/AGENTS.md \
              game-template/.github/copilot-instructions.md \
              "game-template/$DOC_LIST" LICENSE.md; do
    if [[ ! -f "${SCRIPT_DIR}/${source}" ]]; then
        echo "Error: required source not found: ${SCRIPT_DIR}/${source}" >&2
        exit 1
    fi
done

docs=()
while IFS= read -r doc || [[ -n "$doc" ]]; do
    doc="${doc%$'\r'}"
    [[ -z "$doc" || "${doc:0:1}" == "#" ]] && continue
    if [[ ! -f "$SCRIPT_DIR/docs/$doc" ]]; then
        echo "Error: required document not found: $doc" >&2
        exit 1
    fi
    docs+=("$doc")
done < "$TEMPLATE_DIR/$DOC_LIST"

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
echo "Documentation profile: $PROFILE"
echo "Current engine documentation: $DEST/docs/lua-api.md"
echo "Run your game with your Lua-enabled engine build:"
printf '  cd %q && /path/to/mini-mbm --scene main.lua\n' "$DEST"
