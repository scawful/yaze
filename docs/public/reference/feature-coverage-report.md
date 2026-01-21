# Feature & Test Coverage Report (v0.5.3)

This report summarizes feature status and persistence behavior across the
desktop app (yaze), z3ed CLI, and the web/WASM preview, and maps those features
to current automated test coverage. Status levels follow the desktop rubric:
Stable = reliable core workflows, Beta = usable but incomplete, Experimental = WIP.
As of v0.5.3, app data is consolidated under `~/.yaze` on desktop/CLI and
`/.yaze` in the web build (IDBFS), with legacy migrations from AppData/Library/XDG.

## Desktop App (yaze)

| Feature | State | Save/Load & Persistence |
| --- | --- | --- |
| Project files (.yaze) | Stable | Project metadata stored in the .yaze file; recent project list persisted. |
| ROM load/save | Stable | ROM loaded from disk; save writes ROM; timestamped backups when enabled. |
| Overworld Editor | Stable | Overworld edits persist to ROM; version-gated for vanilla/v2/v3. |
| Dungeon Editor | Stable | Room objects/tiles/palettes persist to ROM; shared undo/redo. |
| Palette Editor | Stable | Palette changes persist to ROM; JSON import/export is TODO. |
| Graphics Editor | Beta | Tile/sheet edits persist to ROM; screen tooling is WIP. |
| Sprite Editor | Stable | Sprite edits persist to ROM. |
| Message Editor | Stable | Text edits persist to ROM. |
| Screen Editor | Experimental | WIP; persistence may be incomplete. |
| Hex Editor | Beta | Direct ROM byte edits persist; search UX is incomplete. |
| Assembly/Asar | Beta | Patches apply to ROM; project file editor is incomplete. |
| Emulator | Beta | Runtime-only; save-state format exists but UI is not fully wired. |
| Music Editor | Experimental | Serialization incomplete; some edits may not save. |
| Agent UI | Experimental | Chat history, profiles, and sessions stored under `~/.yaze/agent`. |
| Settings/Shortcuts | Beta | Settings + shortcuts UI wired; persistence stored in `~/.yaze` layouts/workspaces. |
| Panel Layouts/Workspaces | Beta | Layout presets stored under `~/.yaze/layouts` and `~/.yaze/workspaces`. |

## z3ed CLI

| Feature | State | Save/Load & Persistence |
| --- | --- | --- |
| ROM read/write/validate | Stable | Operates directly on ROM file. |
| ROM snapshots/restore | Stable | Snapshot/restore ROM state in project-local `.yaze/snapshots`. |
| Doctor suite | Stable | Diagnostics; optional fix output to file. |
| Editor automation | Stable | Writes changes to ROM. |
| Test discovery/run/status | Stable | Structured output; no ROM required. |
| Agent workflows | Stable | Proposals/policies/sandboxes stored under `~/.yaze/*`. |
| TUI/REPL | Stable | Interactive sessions; REPL supports session save/load. |
| AI providers | Stable | Ollama/Gemini/OpenAI/Anthropic with keys or local server. |

## Web/WASM Preview

| Feature | State | Save/Load & Persistence |
| --- | --- | --- |
| ROM load | Working | Drag/drop or picker; stored in `/.yaze/roms` (IndexedDB). |
| Auto-save | Preview | Auto-save to browser storage when working. |
| Download ROM | Working | Download modified ROM to disk. |
| Overworld/Dungeon/Palette/Graphics/Sprite/Message | Preview | Editing incomplete; persists via browser storage + download. |
| Hex Editor | Working | Direct ROM editing; persisted in browser storage. |
| Asar patching | Preview | Basic assembly patching. |
| Emulator | Not available | No emulator in web build. |
| Collaboration | Experimental | Requires server; persistence depends on server storage configuration. |
| AI features | Preview | Requires AI-enabled collaboration server. |

## Persistence Summary

- Desktop/CLI: app data stored under `~/.yaze` with migration from legacy paths.
- Desktop: primary persistence is ROM save to disk; backups on save when enabled.
- Emulator: save-state format exists, but UI wiring is incomplete.
- CLI: proposals/policies/sandboxes stored under `~/.yaze`; snapshots/restore for ROM state.
- Web: `/.yaze` in IndexedDB; download for durable backups; browser storage can be cleared.

## Automated Test Coverage (Current)

- Unit: core ROM and data structures, gfx conversions, emulator components, CLI/agent services.
- Integration: editor systems (dungeon/overworld/tile16), AI runtime, emulator services, audio, Asar.
- E2E: GUI smoke + dungeon/overworld workflows, editor smoke tests
  (Graphics/Sprite/Message/Music/Save States), emulator stepping, multimodal AI.
- ROM-dependent: full ROM flows (Asar patching, upgrades, end-to-end ROM edits).
- Web/WASM: platform tests + Playwright smoke that runs the debug API suite.

### Test Inventory (Targets + Labels)

- Targets: `yaze_test` (unit/integration), `yaze_test_gui`, `yaze_test_experimental`,
  `yaze_test_rom_dependent`, `yaze_test_benchmark`, `z3ed --self-test`.
- CTest labels: `stable`, `gui`, `headless_gui`, `z3ed`, `rom_dependent`,
  `experimental`, `benchmark`.
- Key locations: `test/unit/`, `test/integration/`, `test/e2e/`,
  `test/platform/wasm_*`, `src/web/tests/wasm_debug_api_tests.js`.

## Coverage Gaps (Observed)

- Limited GUI/E2E coverage depth for Graphics, Sprite, Message, Music,
  Screen, and Settings flows (smoke coverage added, workflow depth pending).
- Emulator save-state UI has smoke coverage; full save/load workflows pending.
- Settings/project manager and layout serialization not fully exercised.
- `.yaze` migration and path normalization lack targeted regression coverage.
- Web build has automated debug API smoke; broader browser CI still light.

### Newly Identified (v0.5.4 Scan)

- **Palette serialization**: JSON import/export not implemented (`palette_group_panel.cc:529,534`)
- **Music bank persistence**: SaveInstruments/SaveSamples return UnimplementedError (`music_bank.cc:925,996`)
- **Screen editor operations**: Undo/Redo/Cut/Copy/Paste stubs (`screen_editor.h:46-52`)
- **Workspace layout**: Save/Load/Reset are TODOs (`workspace_manager.cc:18,26,34`)
- **Platform backend**: Minimal factory tests only (`window_backend_test.cc` - 38 lines)
- **Room object types**: 12+ unknown object types in `room_object.h` need verification
- **CRC32 calculation**: Stubbed with 0 in ASAR wrapper (`asar_wrapper.cc:330,501`)

## Coverage Plan (v0.5.x)

1) [DONE] Add GUI smoke tests for Graphics, Sprite, Message, and Music editors.
2) [DONE] Add emulator save-state UI smoke coverage (workflow tests pending).
3) [TODO] Add settings/layout serialization tests (workspace presets + shortcuts).
4) [TODO] Add `.yaze` migration and path normalization tests (desktop + CLI).
5) [TODO] Expand CLI command coverage for doctor and editor automation commands.
6) [DONE] Promote WASM debug API checks into CI (automated browser run).
7) [TODO] Add ROM-dependent tests for version-gated overworld saves and dungeon persistence.
8) [TODO] Add web storage regression checks (IDBFS sync + file manager flows).

### v0.5.4 Test Additions

9) [TODO] Add palette JSON round-trip tests (`test/unit/palette_json_test.cc`)
10) [TODO] Add workspace layout serialization tests (`test/integration/workspace_test.cc`)
11) [TODO] Add BRR codec unit tests (`test/unit/brr_codec_test.cc`)
12) [TODO] Add music bank save tests (`test/integration/music_bank_test.cc`)
13) [TODO] Expand platform backend tests for SDL2/SDL3 feature parity
