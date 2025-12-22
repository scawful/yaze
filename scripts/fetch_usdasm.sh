#!/usr/bin/env bash
set -euo pipefail

REPO_URL="${USDASM_REPO_URL:-https://github.com/spannerisms/usdasm.git}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="${USDASM_DIR:-$ROOT_DIR/assets/asm/usdasm}"

if [ -d "$TARGET_DIR/.git" ]; then
  echo "usdasm already present at $TARGET_DIR"
  exit 0
fi

if [ -e "$TARGET_DIR" ]; then
  echo "Removing existing path at $TARGET_DIR"
  rm -rf "$TARGET_DIR"
fi

mkdir -p "$(dirname "$TARGET_DIR")"
git clone --depth 1 "$REPO_URL" "$TARGET_DIR"
echo "usdasm fetched to $TARGET_DIR"
