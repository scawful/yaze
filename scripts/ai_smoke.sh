#!/usr/bin/env bash
# Cross-platform AI/CLI/emu smoke test. Builds core AI targets, then
# sanity-checks z3ed and yaze_emu in mock/headless mode.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <preset> [config]" >&2
  echo "Example: PRESET=mac-ai $0 mac-ai Release" >&2
  exit 1
fi

PRESET="$1"
BUILD_CONFIG="${2:-Release}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=== Configure + build ($PRESET, config=$BUILD_CONFIG) ==="
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --config "$BUILD_CONFIG" --target yaze z3ed yaze_emu

BIN_DIR="$ROOT_DIR/build/bin"
if [[ ! -x "$BIN_DIR/z3ed" && -x "$BIN_DIR/$BUILD_CONFIG/z3ed" ]]; then
  BIN_DIR="$BIN_DIR/$BUILD_CONFIG"
fi

Z3ED_BIN="$BIN_DIR/z3ed"
EMU_BIN="$BIN_DIR/yaze_emu"

if [[ ! -x "$Z3ED_BIN" ]]; then
  echo "ERROR: z3ed binary not found in $BIN_DIR" >&2
  exit 1
fi
if [[ ! -x "$EMU_BIN" ]]; then
  echo "ERROR: yaze_emu binary not found in $BIN_DIR" >&2
  exit 1
fi

echo "=== z3ed --version ==="
AI_PROVIDER=mock "$Z3ED_BIN" --version

echo "=== z3ed agent simple-chat (mock ROM) ==="
AI_PROVIDER=mock "$Z3ED_BIN" agent simple-chat --mock-rom --prompt "ping"

echo "=== yaze_emu headless sanity (mock ROM frames) ==="
AI_PROVIDER=mock "$EMU_BIN" --emu_no_gui --emu_rom assets/zelda3.sfc --emu_frames=10 --emu_audio_off --emu_max_frames=20

echo "✓ AI smoke test completed for preset $PRESET"
