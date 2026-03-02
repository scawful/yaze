# Roadmap

**Last Updated: March 2, 2026**

This roadmap tracks upcoming releases and major ongoing initiatives.

---

## Current Focus (v0.7.0)

0.7.0 shipped iOS Remote Control, themed widgets, and desktop HTTP API.
The release is being finalized with editor feature completion before
moving to 0.8.0. See `docs/internal/plans/0.7.0-feature-completion.md`
for the detailed task breakdown and agent assignments.

**0.7.0 completion priorities:**
- P0 remaining: Screen undo/redo, Desktop BPS export
- P0 completed: Tile16 palette/render fix, Message replace, Sprite undo/redo
- P1 remaining: Tracker stubs, OW item deletion fix, CLI palette commands
- P1 completed: Overworld usage statistics card data wiring
- P2 (deferred to 0.8.0 if needed): Persistent scratch pad, eyedropper, SPC import

Four parallel workstreams continue from the 0.6.x cycle.

### Track A: Editor Stability & ZScream Parity

Core editors must reach feature parity with ZScream (the established ALTTP editor)
for reliable ROM hacking. ZScream is the stability benchmark.

**Dungeon Editor** (Beta)
- ✅ 3-phase undo (objects, collision, water fill)
- ✅ Entity drag-drop with selection inspector
- ✅ Custom collision editor with JSON import/export
- ✅ ROM write fence stack
- 🟡 **Object tile counts**: yaze hardcodes 8 tiles per object; ZScream uses object-specific counts (4-242). Complex objects (altars, carpets, large platforms) render incorrectly.
- 🟡 Object preview rendering (stubbed)
- 🟡 12+ unknown dungeon object types need verification
- 🟡 Visual discrepancies in specific objects (vertical rails, doors)
- 🟡 Room object type identification incomplete
- 🟡 ASM export (deferred)

**Overworld Editor** (Beta)
- ✅ Batched undo with paint merge semantics
- ✅ SharedClipboard copy/paste
- ✅ Fill Screen (32x32 tile screen)
- 🟡 Paste undo tracking (not captured in undo stack)
- ✅ Tile16 palette rendering (pixel transform + per-quadrant metadata)
- ✅ Tile16 renderer extracted to `zelda3::RenderTile16BitmapFromMetadata` + unit tests
- ✅ Tile8 usage index extracted to shared `zelda3` service + unit tests
- ✅ Usage statistics card now uses real overworld map data (no placeholder zeros)
- 🟡 Overworld sprite workflow incomplete
- 🟡 Item deletion only hides, doesn't remove
- 🟡 Export file dialog not implemented
- 🟡 **Persistent scratch pad**: ZScream saves `ScratchPad.dat`; yaze scratch is session-only
- 🟡 **Eyedropper tool**: no dedicated tool/shortcut (ZScream has right-click sampling)

**ZScream Parity Targets**

| Feature | ZScream | yaze | Priority |
|---------|---------|------|----------|
| Object-specific tile counts | ✅ Per-object (4-242) | ❌ Hardcoded 8 | High |
| Persistent scratch pad | ✅ `ScratchPad.dat` | ❌ Session-only | Medium |
| Eyedropper tool | ✅ Right-click sampling | ❌ Missing | Medium |
| ZScream project import | ✅ Native format | ❌ Not parsed | Low |
| Selection UX (marquee, context menus) | ✅ Mature | 🟡 Functional, needs validation | Medium |
| Room header editing | ✅ 14-byte headers | ✅ Parity | — |
| Entrance/exit editing | ✅ Full support | ✅ Parity | — |
| Sprite placement | ✅ In-room editing | ✅ Parity | — |
| Chest/door editing | ✅ Full support | ✅ Parity | — |
| Tile16 editor | ✅ 16x16 composites | ✅ Parity (palette fix landed) | — |
| Palette editor | ✅ All contexts | ✅ Parity | — |

**Music Editor** (Beta)
- ✅ Undo/redo with per-song snapshots
- ✅ Tracker + piano roll + instrument/sample editors
- ✅ ASM export/import
- 🟡 Event clipboard (copy/paste selected notes) not implemented
- 🟡 SaveInstruments (`music_bank.cc:925`)
- 🟡 SaveSamples with BRR encoding (`music_bank.cc:996`)

**Screen Editor** (WIP)
- ✅ Load/Save works for 5 screen types
- ❌ Undo/Redo/Cut/Copy/Paste all return `UnimplementedError` (`screen_editor.h:46-52`)

**Sprite Editor** (Beta)
- ✅ Vanilla sprite viewer (OAM rendering, sheet loading)
- ✅ ZSprite animation playback and property editing
- ✅ Undo/Redo implemented (snapshot-based)
- ❌ Copy/Paste stubbed

**Memory Editor** (WIP)
- ✅ Hex viewing
- ❌ Search not implemented

### Track B: Oracle of Secrets Integration

Yaze as the primary development and debugging tool for Oracle of Secrets.

**Mesen2 Socket Client** (Production)
- ✅ Full C++ client: connect, memory read/write, CPU state, breakpoints, trace, disassemble
- ✅ 9 z3ed CLI commands (`mesen-gamestate`, `mesen-sprites`, `mesen-cpu`, etc.)
- ✅ Auto-discovery via `MESEN2_SOCKET_PATH` or `/tmp/mesen2-*.sock`
- 🟡 EventLoop background thread not processing subscriptions
- 🟡 Bulk memory read optimization for cartographer

**Oracle Panels** (Production)
- ✅ State Library Panel: load/verify/deprecate save states from manifest
- ✅ Progression Dashboard: crystal tracker, game phase, dungeon grid, SRAM import
- ✅ Story Event Graph: interactive node canvas with predicate evaluation
- 🟡 Annotation Overlay Panel: registration only, no implementation

**Core Oracle Data** (Production)
- ✅ `OracleProgressionState`: SRAM parsing, crystal bitfield, game phase
- ✅ `StoryEventGraph`: JSON loading, auto-layout, predicate evaluation

**AI Debugging Scripts** (Production)
- ✅ sentinel.py: soft lock watchdog (B007/B009, INIDISP, transition stagnation)
- ✅ crash_dump.py: trace capture + symbol resolution
- ✅ profiler.py: CPU hotspot sampling
- ✅ fuzzer.py: chaos monkey testing
- ✅ state_query.py: semantic game state queries
- 🟡 memory_cartographer.py: works but slow (byte-by-byte reads)
- 🟡 code_graph.py: partial ASM call graph

**Next Steps:**
- Wire live SRAM reads into Progression Dashboard (currently demo data on iOS)
- Implement AnnotationOverlay for room-level debug annotations
- EventLoop thread for real-time breakpoint/event callbacks
- Bulk `READ_BLOCK` in memory_cartographer for performance

### Track C: iOS/macOS App

Mobile testing and review companion for desktop development.

**Build System** (Functional)
- ✅ CMake presets: `ios-debug`, `ios-sim-debug`, `ios-release`
- ✅ Xcodegen from `project.yml`
- ✅ `libyaze_ios_bundle.a` compiles (988MB)
- ✅ Build scripts: `build-ios.sh`, `xcodebuild-ios.sh`

**Platform Backend** (Functional)
- ✅ Metal rendering via MTKView + ImGui
- ✅ Touch input: single-touch, stylus/Pencil, pinch-zoom, two-finger pan, long-press
- ✅ Safe area handling (notch/dynamic island)
- ✅ iOS window backend (`ios_window_backend.mm`)
- ⚠️ Multi-touch is single-primary only (cursor jumps with multi-finger)
- ❌ Audio not implemented (`GetAudioDevice()` returns 0)

**SwiftUI App** (Beta scaffold)
- ✅ Glass-morphism overlay toolbar with ROM picker
- ✅ Oracle Tools Tab (annotations, progression, story events)
- ✅ Document-based app with `.yazeproj` bundles + iCloud sync
- ✅ Settings persistence, AI host management, remote build client
- ✅ Keyboard shortcuts (Cmd+O, Cmd+Shift+P, etc.)
- ⚠️ Progression Dashboard reads demo data, not live SRAM
- ❌ No emulator integration on device

**C++/Swift Bridge** (Functional)
- ✅ ROM loading, project management, panel browsing
- ✅ Oracle progression state + story events JSON
- ✅ Touch scale and safe area inset coordination

**Next Steps:**
- Connect progression dashboard to real SRAM data (`.srm` file parsing)
- Audio pipeline (SDL2 audio → Metal audio session)
- Multi-touch refinement (track multiple touch points)
- Add `PROJECT.toml` iOS platform listing
- TestFlight builds for device testing workflow

### UI Polish (cross-cutting)

- Viewport-relative dialog sizing (migrate 30+ hardcoded `ImVec2`)
- Context menu unification (legacy → declarative)
- Panel simplification (merge Favorites into Pinned)
- Layout serialization (save/load/reset ImGui docking layouts)

### Platform & Performance (cross-cutting)

- Slow shutdown fix (graphics arena ordering)
- CRC32/ASAR checksum completion
- Windows MSVC C++23 alignment (fixed in v0.6.0)
- GCC `std::ranges` compatibility (fixed in v0.6.0)

---

## Deferred

- WASM proposal system completion

---

## Future (v0.8.0+)

- **SDL3 Migration**: GPU-based rendering (backend infrastructure exists, needs editor porting)
- **Plugin Architecture**: Community extensions framework
- **Enhanced Memory Editor**: Search, data interpretation, disassembly view
- **Documentation Overhaul**: Auto-generated C++ API docs, user guide
- **Clipboard parity**: Cut/Copy/Paste/Find for Graphics, Screen editors
- **Music Editor**: SPC/MML import, multi-segment tracker, event clipboard
- **Project lifecycle**: `.yaze`/`.yazeproj` file loading, session save/restore

---

## Release History

### v0.7.0 (February-March 2026)
- iOS Remote Control: Bonjour discovery, Remote Room Viewer, Command Runner
- Themed widget system (BeginThemedTabBar/EndThemedTabBar)
- Desktop HTTP API: command execution, catalog, annotation CRUD
- Tile16 editor palette rendering fix (ZScream-parity pixel transform)
- Tile16 renderer/usage index service extraction with dedicated unit coverage
- Message editor find/replace + replace-all implementation
- Sprite editor undo/redo with snapshot-based action support
- Overworld usage statistics card data wiring
- 6-phase refactoring complete (EditorManager split, OverworldEditor decomposition)
- Sprite undo/redo, Screen undo/redo, Desktop BPS export (in progress)

### v0.6.0 (February 2026)
- Unified UndoManager in Editor base class
- SNES priority compositing with coverage masks
- Entity drag-drop, custom collision editor, water fill authoring
- Semantic color system, EventBus migration, viewport-relative sizing
- ROM write fence stack, dirty collision save
- Dead code cleanup, doc accuracy audit

### v0.5.6 (February 2026)
- Minecart track editor and collision overlay
- Custom object previews and layer-aware hover/selection
- Headless ImGui initialization for tests

### v0.5.3-v0.5.5 (January 2026)
- WASM/web infrastructure hardening
- Local AI support (LMStudio, OpenAI-compatible servers)
- EditorManager modernization, yaze_core_lib extraction
- Mesen2 debug panel and CLI commands

### v0.4.0 (November 2025)
- Music Editor with tracker, piano roll, instrument/sample editors
- SDL3 backend infrastructure (17 abstraction files)
- EditorManager refactoring (8 specialized managers)
- AI agent tools Phases 1-4
- WASM web port (experimental)

### v0.3.x (October-November 2025)
- Vim mode for simple-chat, autocomplete engine
- Dungeon editor crash backlog resolution
- z3ed learn, Gemini integration
- Tile16 editor refactoring
