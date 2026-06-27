#!/usr/bin/env bash
set -u

CONVERTER="/home/michel/mini-mbm/bin/debug/linux_x86/mesh_legacy_converter"

usage() {
  printf 'Usage: %s <folder>\n' "$0" >&2
}

if [ "$#" -ne 1 ]; then
  usage
  exit 2
fi

folder=$1

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
  find "$folder" -type f \( \
    -name '*.spt' -o \
    -name '*.msh' -o \
    -name '*.tile' -o \
    -name '*.fnt' \
  \) -print0
)
