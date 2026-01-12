# Feature & Test Coverage Report (v0.5.0)

This report summarizes feature status and persistence behavior across the
desktop app, z3ed CLI, and the web/WASM preview, and maps those features to
current automated test coverage. Status levels follow the desktop rubric:
Stable = reliable core workflows, Beta = usable but incomplete, Experimental = WIP.

## Desktop App (yaze)

| Feature | State | Save/Load & Persistence |
| --- | --- | --- |
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
| Agent UI | Experimental | Experimental; persistence is not fully documented. |
| Settings/Project Manager | Beta | Settings/layout serialization has TODOs. |
| Panel Layouts/Workspaces | Beta | Layout presets exist; serialization not fully complete. |

## z3ed CLI

| Feature | State | Save/Load & Persistence |
| --- | --- | --- |
| ROM read/write/validate | Stable | Operates directly on ROM file. |
| ROM snapshots/restore | Stable | Snapshot/restore ROM state (CLI-managed storage). |
| Doctor suite | Stable | Diagnostics; optional fix output to file. |
| Editor automation | Stable | Writes changes to ROM. |
| Test discovery/run/status | Stable | Structured output; no ROM required. |
| Agent workflows | Stable | Proposals stored in XDG data path; commit writes ROM; revert reloads ROM. |
| TUI/REPL | Stable | Interactive sessions; REPL supports session save/load. |
| AI providers | Stable | Ollama/Gemini/OpenAI/Anthropic with keys or local server. |

## Web/WASM Preview

| Feature | State | Save/Load & Persistence |
| --- | --- | --- |
| ROM load | Working | Drag/drop or picker; stored in IndexedDB. |
| Auto-save | Preview | Auto-save to browser storage when working. |
| Download ROM | Working | Download modified ROM to disk. |
| Overworld/Dungeon/Palette/Graphics/Sprite/Message | Preview | Editing incomplete; persists via browser storage + download. |
| Hex Editor | Working | Direct ROM editing; persisted in browser storage. |
| Asar patching | Preview | Basic assembly patching. |
| Emulator | Not available | No emulator in web build. |
| Collaboration | Experimental | Requires server; persistence depends on server storage configuration. |
| AI features | Preview | Requires AI-enabled collaboration server. |

## Persistence Summary

- Desktop: primary persistence is ROM save to disk; backups on save when enabled.
- Emulator: save-state format exists, but UI wiring is incomplete.
- CLI: agent proposals stored in XDG data path; snapshots/restore for ROM state.
- Web: IndexedDB auto-save; download for durable backups; browser storage can be cleared.

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
- Web build has automated debug API smoke; broader browser CI still light.

## Coverage Plan (v0.5.x)

1) [DONE] Add GUI smoke tests for Graphics, Sprite, Message, and Music editors.
2) [DONE] Add emulator save-state UI smoke coverage (workflow tests pending).
3) [TODO] Add settings/layout serialization tests (load/save workspace presets).
4) [TODO] Expand CLI command coverage for doctor and editor automation commands.
5) [DONE] Promote WASM debug API checks into CI (automated browser run).
6) [TODO] Add ROM-dependent tests for version-gated overworld saves and dungeon persistence.
