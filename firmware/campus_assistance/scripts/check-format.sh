#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is required. Install clang-format 16 or newer." >&2
  exit 1
fi

while IFS= read -r -d '' file; do
  clang-format --dry-run --Werror -- "$file"
done < <(
  find firmware/campus_assistance -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.ino' \) -print0
)
