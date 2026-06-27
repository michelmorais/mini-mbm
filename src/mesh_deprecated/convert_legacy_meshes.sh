#!/usr/bin/env bash
set -u

CONVERTER="/home/michel/mini-mbm/bin/debug/linux_x86/mesh_legacy_converter"
VALID_EXTENSIONS="spt msh tile fnt ptl"

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

if [ ! -x "$CONVERTER" ]; then
  printf 'Error: converter is not executable: %s\n' "$CONVERTER" >&2
  exit 2
fi

while IFS= read -r -d '' input_file; do
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
done < <(
  find "$folder" -type f -print0 | while IFS= read -r -d '' found_file; do
    for extension in $extensions; do
      case "$found_file" in
        *."$extension")
          printf '%s\0' "$found_file"
          break
          ;;
      esac
    done
  done
)
