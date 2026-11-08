#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake_file="$repo_root/src/CMakeLists.txt"
docs_file="$repo_root/docs/editors.md"

if [ ! -f "$cmake_file" ]; then
    printf '%s\n' "missing $cmake_file" >&2
    exit 1
fi
if [ ! -f "$docs_file" ]; then
    printf '%s\n' "missing $docs_file" >&2
    exit 1
fi

tmp_cmake=$(mktemp "${TMPDIR:-/tmp}/mini-mbm-editor-cmake.XXXXXX")
tmp_docs=$(mktemp "${TMPDIR:-/tmp}/mini-mbm-editor-docs.XXXXXX")
tmp_cmake_scripts=$(mktemp "${TMPDIR:-/tmp}/mini-mbm-editor-cmake-scripts.XXXXXX")
tmp_docs_scripts=$(mktemp "${TMPDIR:-/tmp}/mini-mbm-editor-doc-scripts.XXXXXX")
trap 'rm -f "$tmp_cmake" "$tmp_docs" "$tmp_cmake_scripts" "$tmp_docs_scripts"' EXIT HUP INT TERM

# Extract the actual shell aliases emitted by the Linux/macOS CMake branches. The same aliases
# appear in both branches, so sort -u makes the comparison platform-independent.
awk '
    /alias [A-Za-z0-9_]+=/ && /--scene/ {
        line = $0
        sub(/^.*alias /, "", line)
        alias = line
        sub(/=.*/, "", alias)
        sub(/^.*--scene /, "", line)
        sub(/[[:space:]].*$/, "", line)
        sub(/^.*\//, "", line)
        sub(/[^A-Za-z0-9_.-].*$/, "", line)
        if (alias != "" && line ~ /\.lua$/)
            print alias "\t" line
    }
' "$cmake_file" | sort -u > "$tmp_cmake"

# The editor inventory uses one row per launch script and lists every shortcut in column three.
awk -F'|' '
    /^\|/ {
        script = $3
        if (script ~ /`[A-Za-z0-9_.-]+\.lua`/) {
            sub(/^[^`]*`/, "", script)
            sub(/`.*$/, "", script)
            print script
        }
    }
' "$docs_file" | sort -u > "$tmp_docs"

cut -f2 "$tmp_cmake" | sort -u > "$tmp_cmake_scripts"
cp "$tmp_docs" "$tmp_docs_scripts"

status=0
missing_scripts=$(comm -23 "$tmp_cmake_scripts" "$tmp_docs_scripts")
extra_scripts=$(comm -13 "$tmp_cmake_scripts" "$tmp_docs_scripts")

if [ -n "$missing_scripts" ]; then
    printf '%s\n' "CMake editor scripts missing from docs/editors.md:" >&2
    printf '%s\n' "$missing_scripts" >&2
    status=1
fi
if [ -n "$extra_scripts" ]; then
    printf '%s\n' "Docs editor scripts not emitted by src/CMakeLists.txt:" >&2
    printf '%s\n' "$extra_scripts" >&2
    status=1
fi

tab=$(printf '\t')
while IFS="$tab" read -r alias script; do
    [ -n "$alias" ] || continue
    pattern='`'"$alias"'`'
    if ! grep -Fq "$pattern" "$docs_file"; then
        printf '%s\n' "CMake alias missing from docs/editors.md: $alias ($script)" >&2
        status=1
    fi
done < "$tmp_cmake"

if [ "$status" -ne 0 ]; then
    exit "$status"
fi

printf 'editor documentation check passed: %s launcher scripts, %s aliases\n' \
    "$(wc -l < "$tmp_cmake_scripts" | tr -d ' ')" \
    "$(wc -l < "$tmp_cmake" | tr -d ' ')"

