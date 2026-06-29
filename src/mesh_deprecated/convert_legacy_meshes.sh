#!/usr/bin/env bash
set -u

VALID_EXTENSIONS="spt msh tile fnt ptl"

# Allow an override via env var `MESH_LEGACY_CONVERTER`.
# Otherwise search common build locations (relative to this script) and PATH.
if [ -n "${MESH_LEGACY_CONVERTER-}" ]; then
  CONVERTER="$MESH_LEGACY_CONVERTER"
else
  script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
  repo_root=$(cd "$script_dir/../.." && pwd)

  candidates=(
    "$repo_root/bin/debug/linux_x86/mesh_legacy_converter"
    "$repo_root/bin/debug/arm64/mesh_legacy_converter"
    "$repo_root/bin/debug/macos_x86/mesh_legacy_converter"
    "$repo_root/bin/debug/macos_arm64/mesh_legacy_converter"
    "$repo_root/bin/debug/mesh_legacy_converter"
    "$repo_root/bin/release/mesh_legacy_converter"
    "/opt/homebrew/bin/mesh_legacy_converter"
    "/usr/local/bin/mesh_legacy_converter"
  )

  CONVERTER=""
  for c in "${candidates[@]}"; do
    if [ -x "$c" ]; then
      CONVERTER="$c"
      break
    fi
  done

  if [ -z "$CONVERTER" ]; then
    # Try PATH
    path_cmd=$(command -v mesh_legacy_converter || true)
    if [ -n "$path_cmd" ]; then
      CONVERTER="$path_cmd"
    fi
  fi
fi

usage() {
  printf 'Usage: %s [-spt] [-msh] [-tile] [-fnt] [-ptl] <folder>\n' "$0" >&2
  printf 'If no extension option is provided, all supported extensions are converted.\n' >&2
}

if [ "$#" -lt 1 ]; then
  usage
  exit 2
fi

extensions=""

while [ "$#" -gt 1 ]; do
  case "$1" in
    -spt|-msh|-tile|-fnt|-ptl)
      extensions="$extensions ${1#-}"
      shift
      ;;
    -*)
      printf 'Error: unsupported option: %s\n' "$1" >&2
      usage
      exit 2
      ;;
    *)
      printf 'Error: folder must be the final argument.\n' >&2
      usage
      exit 2
      ;;
  esac
done

folder=$1

if [ -z "$extensions" ]; then
  extensions=$VALID_EXTENSIONS
fi

if [ ! -d "$folder" ]; then
  printf 'Error: folder does not exist: %s\n' "$folder" >&2
  exit 2
fi

if [ -z "${CONVERTER-}" ] || [ ! -x "$CONVERTER" ]; then
  printf 'Error: mesh_legacy_converter executable not found.\n' >&2
  printf 'Searched common build locations and PATH, or set MESH_LEGACY_CONVERTER env var.\n' >&2
  exit 2
fi

find "$folder" -type f -print0 | while IFS= read -r -d '' input_file; do
  for extension in $extensions; do
      case "$input_file" in
        *."$extension")
          # matched extension: perform conversion for this file
          tmp_file=$(mktemp "/tmp/mesh_legacy_converter.XXXXXX") || exit 1

          printf 'Converting: %s\n' "$input_file"

          if "$CONVERTER" "$input_file" "$tmp_file"; then
            if cp "$tmp_file" "$input_file"; then
              rm -f "$tmp_file"
            else
              status=$?
              rm -f "$tmp_file"
              printf 'Error: failed to replace original file: %s\n' "$input_file" >&2
              exit "$status"
            fi
          else
            status=$?
            rm -f "$tmp_file"
            printf 'Error: converter failed for: %s\n' "$input_file" >&2
            exit "$status"
          fi

          break
          ;;
      esac
    done
done
