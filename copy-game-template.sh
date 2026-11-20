#!/usr/bin/env bash
# copy-game-template.sh — Copy the mini-mbm Lua game template to a new folder.
#
# Usage:
#   ./copy-game-template.sh [--profile <name>] [--engine-bin <path>] <destination-folder>
#
# Example:
#   ./copy-game-template.sh --profile 2d5 --engine-bin /home/michel/mini-mbm/bin/debug/linux_x86 /home/michel/my-new-game

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="${SCRIPT_DIR}/game-template"

usage() {
    echo "Usage: $0 [--profile <2d|2d5|3d|animation|authoring|all>] [--engine-bin <path>] <destination-folder>"
    echo ""
    echo "Creates a standalone Lua game; asks before overwriting a nonempty folder."
    echo "The default profile is 2d. Profiles control which engine docs are copied."
    echo "Profiles: 2d, 2d5, 3d, animation, authoring, all."
    echo "--engine-bin records the local engine binary directory in ignored ENGINE.local.md."
    echo "If omitted, MBM_ENGINE_BIN_DIR or a build under this engine checkout is used."
}

PROFILE="2d"
DEST=""
ENGINE_BIN_DIR="${MBM_ENGINE_BIN_DIR:-}"
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
        --engine-bin)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "Error: --engine-bin requires a directory." >&2
                usage >&2
                exit 1
            fi
            ENGINE_BIN_DIR="$2"
            shift 2
            ;;
        --engine-bin=*)
            ENGINE_BIN_DIR="${1#--engine-bin=}"
            if [[ -z "$ENGINE_BIN_DIR" ]]; then
                echo "Error: --engine-bin requires a directory." >&2
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

case "$(uname -s 2>/dev/null || echo unknown)" in
    Linux*)  ENGINE_PLATFORM="Linux" ;;
    Darwin*) ENGINE_PLATFORM="macOS" ;;
    MINGW*|MSYS*|CYGWIN*) ENGINE_PLATFORM="Windows" ;;
    *)       ENGINE_PLATFORM="unknown" ;;
esac

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

is_engine_bin_dir() {
    [[ -d "$1" && -x "$1/mini-mbm" ]]
}

if [[ -n "$ENGINE_BIN_DIR" ]]; then
    if ! is_engine_bin_dir "$ENGINE_BIN_DIR"; then
        echo "Error: engine binary directory is not a usable build: $ENGINE_BIN_DIR" >&2
        echo "Expected executable mini-mbm. The standalone lua-5.4.1.exe is optional." >&2
        exit 1
    fi
    ENGINE_BIN_DIR="$(cd -- "$ENGINE_BIN_DIR" && pwd)"
else
    while IFS= read -r -d '' engine_binary; do
        candidate="$(dirname -- "$engine_binary")"
        if is_engine_bin_dir "$candidate"; then
            ENGINE_BIN_DIR="$(cd -- "$candidate" && pwd)"
            break
        fi
    done < <(find "$SCRIPT_DIR/bin" -type f -name mini-mbm -print0 2>/dev/null)
fi

if [[ -n "$ENGINE_BIN_DIR" ]]; then
    :
else
    ENGINE_BIN_DIR="/path/to/mini-mbm/bin/<platform-build>"
fi

# Validate all sources before creating the destination.
for source in game-template/main.lua game-template/AGENTS.md game-template/ENGINE.md \
              game-template/ENGINE.local.example.md \
              game-template/.gitignore \
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
for target in main.lua AGENTS.md CLAUDE.md ENGINE.md ENGINE.local.md ENGINE.local.example.md \
              .gitignore .github/copilot-instructions.md docs/ENGINE-LICENSE.md "${docs[@]/#/docs/}"; do
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
    echo "This overwrites main.lua, agent instructions, ENGINE.local.md and selected docs."
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
cp -- "$TEMPLATE_DIR/main.lua" "$TEMPLATE_DIR/AGENTS.md" "$TEMPLATE_DIR/ENGINE.md" \
    "$TEMPLATE_DIR/ENGINE.local.example.md" "$DEST/"
cp -- "$TEMPLATE_DIR/ENGINE.local.example.md" "$DEST/ENGINE.local.md"
cp -- "$TEMPLATE_DIR/.github/copilot-instructions.md" "$DEST/.github/"

if [[ ! -e "$DEST/.gitignore" ]]; then
    cp -- "$TEMPLATE_DIR/.gitignore" "$DEST/.gitignore"
elif ! grep -Fqx 'ENGINE.local.md' "$DEST/.gitignore"; then
    if [[ -s "$DEST/.gitignore" && "$(tail -c 1 "$DEST/.gitignore")" != $'\n' ]]; then
        printf '\n' >> "$DEST/.gitignore"
    fi
    printf '# Machine-local path to the mini-mbm engine build.\nENGINE.local.md\n' >> "$DEST/.gitignore"
fi

replace_token() {
    local token="$1"
    local value="$2"
    local file="$3"
    value="${value//\\/\\\\}"
    value="${value//&/\\&}"
    value="${value//|/\\|}"
    # Use a backup suffix supported by both GNU sed and the BSD sed shipped
    # with macOS; plain `sed -i` is not portable between them.
    sed -i.bak "s|${token}|${value}|g" "$file"
    rm -f "$file.bak"
}

replace_token "@ENGINE_PLATFORM@" "$ENGINE_PLATFORM" "$DEST/ENGINE.local.md"
replace_token "@ENGINE_BIN_DIR@" "$ENGINE_BIN_DIR" "$DEST/ENGINE.local.md"
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
echo "Engine binary directory: $ENGINE_BIN_DIR"
echo "Local engine configuration: $DEST/ENGINE.local.md (ignored by Git)"
echo "Run your game with your Lua-enabled engine build:"
printf '  cd %q && %q/mini-mbm --scene main.lua --disable_select_monitor --nosplash\n' "$DEST" "$ENGINE_BIN_DIR"
