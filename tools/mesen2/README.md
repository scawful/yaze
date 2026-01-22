# Mesen 2 Integration Guide

`yaze` provides tools to integrate with the Mesen 2 emulator, allowing for advanced debugging workflows.

## 1. Symbol Export (.mlb)

Mesen 2 uses `.mlb` files for labels/symbols. `yaze` can export its internal symbol table to this format.

### CLI Command

```bash
# Export symbols from a project
yaze --headless --rom_file zelda3.sfc --export_symbols output.mlb --symbol_format mesen
```

Tip: Use `--export_symbols_fast` with `--load_symbols` or `--load_asar_symbols`
to export without initializing the UI.

### Automatic Loading

If you name the `.mlb` file the same as your ROM (e.g., `zelda3.sfc` and `zelda3.mlb`) and place them in the same directory, Mesen 2 will automatically load the symbols.

## 2. Lua Script Bridge

A Lua script is provided in `tools/mesen2/yaze_bridge.lua`.

1.  Enable the **API Server** in `yaze`:
    ```bash
    yaze --server --rom_file zelda3.sfc
    ```
    This starts an HTTP server on port 8080.

2.  Load the script in Mesen 2:
    *   Open Mesen 2.
    *   Go to **Debug > Script Window**.
    *   Load `tools/mesen2/yaze_bridge.lua`.

The script currently attempts to connect to `yaze`. Future updates will allow live syncing of:
*   Real-time symbol updates (if you rename a label in `yaze`, it updates in Mesen).
*   PC syncing (click in `yaze` assembly view to move Mesen PC).
*   Watchpoints.

## 3. Recommended Mesen 2 Settings

*   **Debugger > Options > Auto-load .mlb files**: Enabled.
*   **Debugger > Options > Allow 24-bit addresses in symbols**: Enabled (for SNES HiROM/ExHiROM).
