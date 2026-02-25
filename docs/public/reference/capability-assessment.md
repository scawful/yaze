# Yaze Capability Assessment

Last updated: 2026-02-24 (v0.6.1)

An honest assessment of yaze's current capabilities compared to Hyrule Magic and ZScream, the two established ALTTP ROM editors.

## Editor Capability Matrix

### Dungeon Editor (~75% parity with ZScream)

| Feature | yaze | ZScream | Hyrule Magic |
|---------|------|---------|--------------|
| Room viewing + rendering | Yes | Yes | Yes |
| Tile object placement/move/delete | Yes | Yes | Yes |
| Sprite placement/move/delete | Yes | Yes | Yes |
| Door placement/move/delete | Yes | Yes | Yes |
| Object limit enforcement (placement-time) | Yes (400/64/16) | Save-time only | Save-time only |
| Ghost preview with capacity indicators | Yes (color-coded) | No | No |
| Multi-object selection (marquee + shift/ctrl) | Yes | Yes | Partial |
| Object z-ordering (front/back) | Yes | Yes | Yes |
| Object layer assignment (BG1/BG2/BG3) | Yes | Yes | Yes |
| Layer visibility toggles | Yes (compact bar) | Yes | Yes |
| Room header editing (palette, blockset, etc.) | Partial (settings panel) | Full | Full |
| Custom collision editing | Yes (JSON import/export) | Limited | No |
| Water-fill editing | Yes (JSON import/export) | No | No |
| Minecart/track rail tools | Yes (audit + generation) | No | No |
| Undo/redo | Yes | Yes | Limited |
| Clipboard (copy/paste objects) | Yes | Yes | No |
| Workbench single-room mode | Yes | N/A | N/A |
| Multi-room tab view | Yes | Yes | Yes |
| Adjacent room navigation (Ctrl+arrows) | Yes | No | No |
| Room save to ROM | Yes | Yes | Yes |
| Sprite graphics rendering (actual tiles) | No (colored boxes) | Yes | Yes |
| Object tile preview in ghost | Yes (rendered bitmap) | No | No |

### Overworld Editor (~60-65% parity with ZScream)

| Feature | yaze | ZScream | Hyrule Magic |
|---------|------|---------|--------------|
| Map viewing (all 160 maps) | Yes | Yes | Yes |
| Tile16 painting (draw mode) | Yes | Yes | Yes |
| Fill tool (flood fill) | Yes | Yes | Yes |
| Tile16 selector with search/filter | Yes (hex jump + range filter) | Basic | Basic |
| Tile16 editor (compose from tile8s) | Yes | Yes | No |
| Tile8 selector | Yes | Yes | Yes |
| Tile hover preview (ID + zoom) | Yes | No | No |
| Entrance editing (visual) | Yes | Yes | Yes |
| Exit editing (visual) | Yes | Yes | Yes |
| Item placement editing | Yes | Yes | Yes |
| Overworld sprite editing | Yes | Yes | Yes |
| Transport/whirlpool editing | Yes | Yes | No |
| Music area editing | Yes | Yes | No |
| Map properties (palette, gfx groups) | Yes | Yes | Yes |
| Scratch space (tile staging) | Yes | No | No |
| ZSCustomOverworld support | Yes | Yes (native) | No |
| Save overworld to ROM | Yes | Yes | Yes |
| Map export/graph visualization | Yes (CLI) | No | No |

### Graphics Editor (~35% parity)

| Feature | yaze | ZScream | Hyrule Magic |
|---------|------|---------|--------------|
| GFX sheet viewing | Yes | Yes | Yes |
| Palette editing | Yes (dedicated editor) | Yes | Yes |
| Pixel-level 8x8 tile editing | Basic | Yes | Yes |
| GFX group management | Viewing | Full editing | Full editing |
| Animated tile preview | No | Yes | No |
| GFX import/export | No | Yes | No |

### Other Editors

| Editor | yaze | ZScream | Hyrule Magic |
|--------|------|---------|--------------|
| Message/text editor | Yes (import/export/encode/decode) | Yes | Limited |
| Music editor | Yes (SPC playback, track editing) | No | No |
| Sprite editor (data/properties) | Yes | Yes | Yes |
| Screen editor (title/file select) | Basic | Yes | Yes |
| Palette editor (standalone) | Yes | Yes | Yes |
| Assembly editor (integrated) | Yes | No | No |
| Hex editor (integrated) | Yes | No | No |

## Unique to yaze (no competitor equivalent)

### CLI Automation (z3ed — 127 commands)

No other ALTTP editor has a command-line interface. yaze's `z3ed` provides:
- **27 dungeon commands** — room inspection, object/sprite manipulation, collision editing, water-fill, minecart audit
- **10 overworld commands** — map description, entrance/exit/item/sprite listing, tile search, graph export
- **13 graphics commands** — hex read/write/search, palette analysis, sprite properties
- **10 ROM commands** — info, validate, diff, compare, address resolve, symbol find
- **4 Oracle commands** — menu validation, smoke check, preflight
- **3 project commands** — bundle verify, pack, unpack
- **12 emulator commands** — step, breakpoint, memory read/write, register inspect
- **7 GUI automation commands** — click, type, wait, assert, discover, screenshot

All commands support `--format=json` for machine consumption and can be scripted.

### Project Bundle System (.yazeproj)

- Directory-based portable project format
- `project-bundle-verify` — structural integrity + ROM hash validation
- `project-bundle-pack` — zip archive for sharing (cross-platform safe paths)
- `project-bundle-unpack` — extract with path traversal protection + dry-run preview
- Works across macOS, iOS, Windows, Linux
- iCloud sync support for Mac/iPad workflow

### Oracle of Secrets Tooling

Purpose-built validation for the Oracle romhack:
- `oracle-smoke-check` — D4 water system, D6 minecart, D3 prison structural validation
- `dungeon-oracle-preflight` — water-fill region, custom collision maps, required room checks
- `oracle-menu-validate` — ASM menu data integrity
- `oracle-menu-index` — menu asset scanning
- Custom collision JSON import/export workflow
- Water-fill JSON import/export workflow

### Integrated Development Environment

- Integrated assembly editor with symbol resolution
- Integrated hex editor with ROM address navigation
- Emulator integration (Mesen2 socket API) with breakpoints, stepping, memory inspection
- AI agent chat with ROM context
- Multi-ROM sessions (up to 8 concurrent)

### Testing Infrastructure

- 1,399 unit tests across 211 test suites
- ImGui Test Engine integration for GUI testing
- Protocol audit system for agent coordination verification
- Oracle-specific regression test suite

## Where yaze is behind (honest gaps)

1. **Sprite graphics rendering** — Dungeon sprites show as colored rectangles instead of their actual SNES tiles. This is the single largest visual gap for room authoring.

2. **GFX sheet editing depth** — ZScream and Hyrule Magic allow individual 8x8 tile editing, GFX import/export, and animated tile preview. yaze's graphics editor is primarily a viewer.

3. **Room header completeness** — Some room header fields may not be exposed in the UI (needs verification against ZScream's full property list).

4. **Screen editors** — Title screen and file select screen editors are basic compared to competitors.

5. **GFX import/export** — No way to import/export graphics sheets or individual tiles.

## Assessment Summary

| Area | Parity | Notes |
|------|--------|-------|
| Dungeon editing | ~75% | Strong interaction model, unique collision/water-fill tools. Gap: sprite rendering. |
| Overworld editing | ~60-65% | Functional painting + entity editing. Gap: some polish vs ZScream. |
| Graphics editing | ~35% | Viewer + palette editor. Gap: pixel editing, import/export. |
| CLI/Automation | No competitor | 127 commands, JSON output, full scripting capability. |
| Project management | Ahead | .yazeproj bundles, cross-platform, hash verification. |
| Oracle tooling | Unique | Purpose-built validation suite, no equivalent exists. |
| Platform support | Ahead | macOS/Linux/Windows/iOS/WASM vs Windows-only competitors. |
| Testing/reliability | Ahead | 1,399 tests, CI automation, protocol audit. |
