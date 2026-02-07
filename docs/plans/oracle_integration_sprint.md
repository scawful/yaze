# Oracle of Secrets Integration Sprint

**Created:** 2026-02-06
**Updated:** 2026-02-07
**Context:** Features that directly unblock or accelerate Oracle of Secrets ROM hack development
**Status:** Detailed implementation plan — ready for execution

---

## Goals

- Make yaze a first-class editor for Oracle of Secrets content (messages, dungeon rooms, tags, minecart tracks) without accidentally corrupting ASM-owned ROM space.
- Reduce time-to-repro and time-to-fix for runtime issues by tightening yaze <-> Mesen2-OOS feedback loops.
- Turn Oracle metadata (`hack_manifest.json`) into actionable UI: safety warnings, debugging views, and toggleable feature flags.

## Non-Goals

- Full Mesen2 window embedding (explicitly not viable; see below).
- Solving Oracle runtime bugs inside yaze. Goal is to *accelerate* debugging, not replace Mesen2 or the ROM build pipeline.
- Replacing the Oracle build scripts or z3dk analysis; yaze should integrate with them, not fork them.

## Operating Rules (Hard Requirements)

- **No silent ROM writes into ASM-owned regions.**
  Any editor action that would write into a protected region must surface a blocking warning with explicit user confirmation. "Log only" is not acceptable.
- **Respect Hook ABI / register-width hygiene as a workflow constraint.**
  Recent Oracle blackout root cause was M/X leakage from a room-load hook; any yaze tooling that encourages ROM patching must make "ownership + ABI expectations" visible.
- **Feature flags are the primary mechanism for experimental work.**
  yaze should make it easier to toggle flags, not easier to land un-gated experimental code.
- **Single source of truth is `hack_manifest.json`.**
  yaze should consume it and surface its implications; do not re-infer ownership via ad-hoc heuristics.

## Cross-Repo Integration Checklist (Oracle <-> yaze)

To keep yaze behavior aligned with the ROM you are actually editing:

- **Manifest freshness:** Ensure yaze is reading the `hack_manifest.json` generated from the same Oracle commit/ROM build you are editing.
- **Protected regions:** Treat "protected regions" as authoritative. If yaze writes into them, it must warn (Feature 3).
- **Feature flags:** Prefer toggling `Config/feature_flags.asm` (Feature 4) over ad-hoc ROM patching.
- **Source verification:** If an agent tool (or MCP) reports a register-width / ownership issue, verify against local source and the actual ROM map. Do not trust standalone heuristics.

Recommended practice for agents:

- When a task depends on manifest ownership, include in the task card:
  - the Oracle commit hash / ROM version you validated against
  - the `hack_manifest.json` timestamp or generator output used

## Team Execution Model (Subagent-Friendly)

Split into parallel workstreams with minimal cross-talk:

- **WS-A: Message pipeline** (Feature 1, Feature 8)
  Owner profile: yaze UI + text serialization
- **WS-B: ROM safety / write protections** (Feature 3, parts of Feature 4)
  Owner profile: yaze core/editor manager + manifest integration
- **WS-C: Dungeon authoring** (Feature 5, Feature 6, Feature 9)
  Owner profile: dungeon editor + overlays + z3ed CLI
- **WS-D: Mesen2-OOS integration** (Feature 2, Feature 10)
  Owner profile: socket client + debug panels + perf
- **WS-E: QA and acceptance** (cross-cutting)
  Owner profile: build + ROM diff verification + regression checklist

Each workstream should ship in small PR-sized slices (1 feature or 1 vertical slice at a time) with explicit acceptance criteria (see "Acceptance Criteria" below).

## Architecture Overview

All features build on existing yaze infrastructure:
- **HackManifest** (`core/hack_manifest.h/.cc`): 641 hooks, 455 protected regions, 33 feature flags, 150 SRAM vars
- **MesenSocketClient** (`emu/mesen/mesen_socket_client.h/.cc`): 777-line C++ client with full command coverage
- **PanelManager**: EditorPanel interface with session/global scope registration
- **PopupManager**: Modal dialog infrastructure with PopupID constants
- **DungeonCanvasViewer**: 6+ existing overlays following toggle-bool + DrawXxx pattern

---

## Tier 1: Unblocks Multiple Oracle Tasks Today

### Feature 1: Expanded Message Write Path

**Oracle blockers unlocked:** Maiden/essence dialogue, Gossip Stones, Maku Tree hints ($1C5-$1CB), Windmill Guy ($1D5-$1D8), D3 prison messages ($1CC-$1D1), Elder 0x2C split

**Current state:**
- `WriteExpandedTextData()` exists at `message_data.cc:980` and works from CLI
- GUI `SaveExpandedMessages()` at `message_editor.cc:852` writes to separate BIN file, NOT main ROM
- CLI `message-write` command correctly writes to main ROM buffer via `WriteExpandedTextData()`
- GUI can read/edit/display expanded messages in-memory (lines 295-409)

**Root cause:** `SaveExpandedMessages()` uses `expanded_message_bin_` (separate Rom object) instead of the main `rom_` buffer.

**Implementation:**

1. **Modify `SaveExpandedMessages()` in `message_editor.cc:852`:**
   - Instead of writing to `expanded_message_bin_`, collect all expanded message raw strings
   - Call `WriteExpandedTextData(rom_->mutable_data(), GetExpandedTextDataStart(), GetExpandedTextDataEnd(), text_strings)`
   - This mirrors what the CLI does at `message_commands.cc:685`

2. **Wire unified save in `Save()` at `message_editor.cc:807`:**
   - After existing vanilla write logic, add: `if (!expanded_messages_.empty()) { RETURN_IF_ERROR(SaveExpandedMessages()); }`
   - This makes Ctrl+S save both vanilla and expanded messages

3. **Recalculate addresses after write:**
   - After `WriteExpandedTextData()`, iterate `expanded_messages_` and update each `Address` field
   - `pos = GetExpandedTextDataStart(); for msg: msg.Address = pos; pos += msg.Data.size() + 1;`

4. **Add capacity validation UI:**
   - Show remaining bytes in expanded bank below the message list
   - `remaining = GetExpandedTextDataEnd() - current_write_pos`
   - Color-code: green (>50%), yellow (<25%), red (<10%)

**Files modified:**
| File | Change |
|------|--------|
| `src/app/editor/message/message_editor.cc` | Rewrite `SaveExpandedMessages()` to use main ROM; wire into `Save()` |
| `src/app/editor/message/message_editor.h` | Add `int CalculateExpandedBankUsage() const` |

**Complexity:** S | **Risk:** Low (CLI path already validates the write logic)

---

### Feature 2: SRAM Variable Viewer Panel

**Oracle value:** Live debugging of game state, crystal flags, story progression without manual memory reads.

**Current state:**
- HackManifest loads 150 SRAM variables from `hack_manifest.json`
- `MesenSocketClient` has `ReadByte()`, `ReadWord()`, `ReadBlock()` for memory access
- `MesenDebugPanel` exists with Link state + sprite display (100ms refresh)
- No panel shows SRAM variables by name

**Implementation:**

1. **New panel: `SramViewerPanel` (EditorPanel subclass)**
   - File: `src/app/editor/agent/panels/sram_viewer_panel.h/.cc`
   - Register in DungeonEditorV2 or as global panel
   - Interface: `GetId()` = `"debug.sram_viewer"`, category = `"Debug"`

2. **Panel layout:**
   - **Table columns:** Address | Name | Purpose | Current Value | Hex | Actions
   - **Grouping:** Collapsible sections by prefix (Story, Dungeon, Items, Side Quest)
   - **Refresh:** Auto-read via `MesenSocketClient::ReadBlock()` every 200ms when connected
   - **Poke button:** `MesenSocketClient::WriteByte()` for testing

3. **Bitfield expansion for known addresses:**
   - `$7EF37A` (Crystals): Expand into 7 checkboxes with dungeon names (from MEMORY.md bitfield table)
   - `$7EF3C5` (GameState): Show as enum dropdown (0=Start, 1=LoomBeach, 2=KydrogComplete, 3=FaroreRescued)
   - `$7EF3D6` (OOSPROG): Expand into individual story flag bits with names from hack_manifest
   - Config: Bitfield definitions stored in `hack_manifest.json` `sram_variables[].bitfield` array

4. **Socket connection:**
   - Reuse `MesenClientRegistry` for socket discovery
   - Show connection status indicator (green/red dot)
   - "Connect" button with auto-discovery dropdown

**Files created/modified:**
| File | Change |
|------|--------|
| `src/app/editor/agent/panels/sram_viewer_panel.h` | NEW — EditorPanel subclass |
| `src/app/editor/agent/panels/sram_viewer_panel.cc` | NEW — Table rendering, bitfield expansion, socket I/O |
| `src/app/editor/dungeon/dungeon_editor_v2.cc` | Register panel in `Initialize()` |
| `src/core/hack_manifest.h` | Add `BitfieldDefinition` struct to `SramVariable` |

**Complexity:** M | **Risk:** Low (read-only socket operations, existing client)

---

### Feature 3: Save-Time Write Conflict Warnings

**Oracle value:** Prevents silent overwrite of ASM hooks when saving dungeon/overworld edits. Currently, save blows away hook code with no warning.

**Current state:**
- `AnalyzePcWriteRanges()` API exists in HackManifest and returns `WriteConflict` structs
- Called in `DungeonEditorV2::SaveRoomData()` lines 535-619, but only `LOG_WARN`s
- `PopupID::kRomWriteConfirm` (line 168 of popup_manager.cc) provides exact template
- `DrawRomWriteConfirmPopup()` at line 1410 shows role/policy/hash with Save/Cancel buttons

**Implementation:**

1. **Add PopupID constant:**
   - `popup_manager.h`: `constexpr const char* kWriteConflictWarning = "Write Conflict Warning";`

2. **Register popup in `PopupManager::Initialize()`:**
   - After line 170: register `kWriteConflictWarning` with `DrawWriteConflictWarningPopup()` callback

3. **Add EditorManager state:**
   ```
   editor_manager.h:
     std::vector<core::WriteConflict> pending_write_conflicts_;
     bool bypass_write_conflict_once_ = false;
   ```

4. **Pre-save check in `EditorManager::SaveRom()` (after line 2532):**
   - Collect write ranges from all dirty editors via new `CollectWriteRanges()` methods
   - Call `manifest.AnalyzePcWriteRanges(all_ranges)`
   - If conflicts and `!bypass_write_conflict_once_`: store conflicts, show popup, return `CancelledError`
   - If bypass: clear flag, proceed with save

5. **Implement `DrawWriteConflictWarningPopup()`:**
   - List each conflict: icon + PC address + ownership type + module name
   - "Save anyway (changes lost on build)" button → sets bypass flag → retries save
   - "Cancel" button → hides popup
   - Warning text: "These addresses are owned by ASM hooks and will be overwritten on next build"

6. **Add `CollectWriteRanges()` to editors:**
   - `DungeonEditorV2::CollectWriteRanges()` — aggregate header/object/sprite ranges for dirty rooms
   - `OverworldEditor::CollectWriteRanges()` — overworld map ranges

**Files modified:**
| File | Change |
|------|--------|
| `src/app/editor/ui/popup_manager.h` | Add `kWriteConflictWarning` constant |
| `src/app/editor/ui/popup_manager.cc` | Register popup, implement draw function |
| `src/app/editor/editor_manager.h` | Add conflict state fields |
| `src/app/editor/editor_manager.cc` | Pre-save check in `SaveRom()` |
| `src/app/editor/dungeon/dungeon_editor_v2.h/.cc` | Add `CollectWriteRanges()` |

**Complexity:** M | **Risk:** Low (follows existing kRomWriteConfirm pattern exactly)

---

## Tier 2: Significant Workflow Improvements

### Feature 4: Feature Flag Editor Panel

**Oracle value:** Toggle `!ENABLE_WATER_GATE_HOOKS`, `!ENABLE_D3_PRISON_SEQUENCE`, etc. without manual ASM editing.

**Current state:**
- HackManifest loads all feature flags with name, value, enabled status, source file
- `Config/feature_flags.asm` is the override file (generated format: `!FLAG = value`)
- `Util/macros.asm` has defaults
- No UI to view or toggle flags

**Implementation:**

1. **New panel: `FeatureFlagEditorPanel`**
   - File: `src/app/editor/agent/panels/feature_flag_editor_panel.h/.cc`
   - Category: "Project" or "Debug"

2. **Panel layout:**
   - Table: Flag Name | Value | Status | Source | Toggle
   - Status color: green=ON, red=OFF, yellow=UNTESTED (detect from `hack_manifest.json` annotations)
   - Toggle: checkbox or ON/OFF button per flag
   - "Save" button: regenerate `Config/feature_flags.asm`

3. **Write-back logic:**
   - Read current `Config/feature_flags.asm`
   - For each changed flag: update `!FLAG = new_value` line
   - Preserve comments and formatting
   - Write file atomically (temp + rename)

4. **Validation:**
   - Show warning icon if flag controls a hook that affects loaded ROM
   - Cross-reference with `AnalyzeWriteRanges()` to show which addresses are affected

**Files created/modified:**
| File | Change |
|------|--------|
| `src/app/editor/agent/panels/feature_flag_editor_panel.h` | NEW |
| `src/app/editor/agent/panels/feature_flag_editor_panel.cc` | NEW |
| `src/core/hack_manifest.h` | Add `SetFeatureFlag()` method |

**Complexity:** S-M | **Risk:** Low (file write only, no ROM changes)

---

### Feature 5: Enhanced Dungeon Canvas Track Overlay

**Oracle value:** Visualize D6 Goron Mines track layout directly in dungeon editor. Currently must cross-reference room data, collision tiles, and sprite positions manually.

**Current state:**
- `DrawTrackCollisionOverlay()` at `dungeon_canvas_viewer.cc:2274` shows colored collision tiles
- `DrawMinecartSpriteOverlay()` at line 2398 shows cart sprite validation (on stop tile or not)
- Track origin markers drawn inline at lines 1826-1869
- `MinecartTrackEditorPanel` provides track data
- `TrackCollisionConfig` from HackManifest defines tile classifications

**Implementation:**

1. **New overlay: `DrawTrackRouteOverlay()`**
   - Draw connected lines between sequential rail tiles showing track paths
   - Use distinct colors per track ID (8 colors, cycling)
   - Arrow heads at start/end showing direction
   - Dashed lines where collision data is missing (gap detection)

2. **Enhanced stop tile visualization:**
   - Existing overlay draws tiles; add directional arrows ($B7=Up, $B8=Down, $B9=Left, $BA=Right)
   - Arrow glyphs rendered via `draw_list->AddTriangleFilled()`
   - Legend updated with direction names

3. **Switch corner highlighting:**
   - $D0-$D3 tiles drawn with distinct pattern (crosshatch or hatched fill)
   - Show which track IDs converge at each switch

4. **Gap detection overlay:**
   - Compare rail object extents against collision data
   - Highlight tiles where object exists but collision is missing (orange warning)
   - Highlight tiles where collision exists but no object (gray info)

5. **Toggle controls:**
   - Add `show_track_routes_overlay_` bool
   - Checkbox in toolbar after existing track controls
   - Respect existing show/hide architecture

**Files modified:**
| File | Change |
|------|--------|
| `src/app/editor/dungeon/dungeon_canvas_viewer.h` | Add overlay toggle + method decl |
| `src/app/editor/dungeon/dungeon_canvas_viewer.cc` | Implement `DrawTrackRouteOverlay()`, enhance stop tile arrows |

**Complexity:** M | **Risk:** Low (additive overlay, follows existing pattern)

---

### Feature 6: Room Tag Editor Panel

**Oracle value:** View all 22+ room tag slots, see which are used/available, assign tags to rooms without memorizing hex values.

**Current state:**
- Room tag combo boxes in dungeon editor (lines 469-688) already show HackManifest labels
- `GetRoomTag()` returns tag info including asm_label, feature_flag, enabled status
- Vanilla tag names from `Zelda3Labels::GetRoomTagNames()`
- `Docs/Technical/Room_Tag_Slots.md` documents the dispatch table

**Implementation:**

1. **New panel: `RoomTagEditorPanel`**
   - File: `src/app/editor/dungeon/panels/room_tag_editor_panel.h/.cc`
   - Category: "Dungeon"

2. **Panel layout:**
   - **Tag dispatch table view:** Grid of 22 slots (0x00-0x3A) showing:
     - Slot index | Vanilla name | ASM label | Feature flag | Status | Rooms using this tag
   - Color-code: green=active, red=disabled, gray=available, yellow=UNTESTED
   - Click slot → shows all rooms using that tag (query ROM room headers)

3. **Quick assign workflow:**
   - Select a tag slot → "Assign to current room" button
   - Dropdown: Tag1 or Tag2
   - Calls `room.set_tag1(tag_idx)` or `room.set_tag2(tag_idx)`

4. **Available slot tracking:**
   - Parse dispatch table at $01CC00-$01CC5A
   - Cross-reference with HackManifest room_tags
   - Show "Available" for unused slots (currently: 0x36 per MEMORY.md)

**Files created/modified:**
| File | Change |
|------|--------|
| `src/app/editor/dungeon/panels/room_tag_editor_panel.h` | NEW |
| `src/app/editor/dungeon/panels/room_tag_editor_panel.cc` | NEW |
| `src/app/editor/dungeon/dungeon_editor_v2.cc` | Register panel |

**Complexity:** M | **Risk:** Low (read-only visualization with optional tag assignment)

---

## Tier 3: Quality of Life

### Feature 7: Progression Flow Visualizer

**Oracle value:** Visual graph showing dungeon completion → NPC reaction chains → Maku Tree hints. Catch narrative gaps without playing through the whole game.

**Implementation:**

1. **Data source:** Parse `Core/progression.asm` tables (crystal count → message ID mappings) + hack_manifest SRAM variables
2. **Graph rendering:** Use ImGui node editor or simple tree layout
   - Nodes: dungeons (D1-D7), NPCs (Maku, Zora, Elder), events (Farore Rescued)
   - Edges: "requires N crystals" or "requires flag X"
3. **Interactive:** Click node → jump to relevant ASM file or message editor
4. **Panel:** Register as "Project" category EditorPanel

**Complexity:** L | **Risk:** Medium (graph layout is non-trivial)

---

### Feature 8: Dialogue Preview with Oracle Formatting

**Oracle value:** See messages as they appear in-game without launching Mesen2. Font atlas is already loaded in MessageEditor.

**Current state:**
- `font_gfx_bitmap_` loaded in `MessageEditor::LoadFontGraphics()`
- `DrawMessagePreview()` renders current message to `current_font_gfx16_bitmap_`
- SNES font rendering already works for the selected message

**Implementation:**

1. **Enhance existing preview:** The preview already renders to bitmap; make it larger and more prominent
2. **Add scroll marker visualization:** Show `[S]` scroll breaks as visual line separators
3. **Character count indicator:** Show remaining chars per line (SNES text box is ~19 chars wide)
4. **Preview panel:** Extract preview rendering into its own sub-panel that can be docked separately

**Files modified:**
| File | Change |
|------|--------|
| `src/app/editor/message/message_editor.cc` | Enhanced preview + scroll break visualization |

**Complexity:** S | **Risk:** Low (extends existing rendering)

---

### Feature 9: z3ed Batch Operations

**Oracle value:** Generate collision data for all D6 rooms in one shot instead of room-by-room CLI commands.

**Implementation:**

1. **CLI command:** `dungeon-generate-track-collision --rooms 0xA8,0xB8,0xD8,0xDA --write`
   - Already has single-room support; add comma-separated multi-room mode
2. **GUI:** "Generate All" button in MinecartTrackEditorPanel
   - Iterates all rooms with rail objects (Object 0x31)
   - Dry-run summary → confirm → write
3. **Report:** Show per-room results (tiles generated, gaps found, errors)

**Files modified:**
| File | Change |
|------|--------|
| `src/cli/handlers/game/dungeon_commands.cc` | Multi-room flag for collision generation |
| `src/app/editor/dungeon/panels/minecart_track_editor_panel.cc` | "Generate All" button |

**Complexity:** S | **Risk:** Low (extends existing single-room pipeline)

---

### Feature 10: Mesen2-OOS Enhanced Integration

**Oracle value:** Reduce app-switching between yaze, Mesen2, and terminal during debugging.

**Current state (already exists):**
- Full C++ socket client: `MesenSocketClient` (777 lines, 55+ commands)
- `MesenDebugPanel` with real-time Link state, sprites, CPU, collision overlay
- `MesenClientRegistry` for multi-instance management
- Python CLI with 89 commands

**What's missing:**

#### 10a. Screenshot Preview Panel
- Poll `MesenSocketClient::Screenshot()` at 10 FPS
- Decode base64 PNG → ImGui texture
- Display in resizable ImGui window
- Overlay: current room ID, Link position, frame count
- **Latency:** ~50-100ms per frame (good for debugging, not real-time play)

#### 10b. SRAM Variable Live Sync
- Wire SRAM Viewer Panel (Feature 2) to auto-refresh via socket
- `ReadBlock($7EF300, 256)` every 200ms → update all SRAM values
- Highlight changed values (flash yellow on change)

#### 10c. Room Warp Button
- In dungeon editor: "Warp to Room" button next to room selector
- Calls `MesenSocketClient::WriteByte()` to set `$A0` (current room) + trigger transition
- Or use Python CLI `navigate --room 0xDA` via `std::system()` or subprocess

#### 10d. Breakpoint-Triggered Snapshots
- Subscribe to breakpoint events via `MesenSocketClient::Subscribe()`
- On breakpoint hit: capture screenshot + CPU state + SRAM snapshot
- Display in a "Breakpoint Log" panel with timeline

**Implementation:**

1. **Screenshot panel** (`mesen_screenshot_panel.h/.cc`):
   - Timer-based polling (configurable 5-30 FPS)
   - Base64 decode → raw RGBA → ImGui texture upload
   - Pause/resume controls
   - Frame counter overlay

2. **Room warp integration:**
   - Add "Warp" button to room header bar in dungeon editor
   - Use `MesenSocketClient::WriteWord(0x7E00A0, room_id)` + poke transition trigger
   - Alternative: invoke Python CLI `navigate` command

3. **Breakpoint log panel:**
   - Event subscription via `Subscribe("breakpoint_hit")`
   - Circular buffer of last 50 breakpoint hits
   - Each entry: timestamp, PC address, symbol name, screenshot thumbnail

**Files created/modified:**
| File | Change |
|------|--------|
| `src/app/editor/agent/panels/mesen_screenshot_panel.h/.cc` | NEW — screenshot preview |
| `src/app/editor/dungeon/dungeon_canvas_viewer.cc` | "Warp to Room" button |
| `src/app/editor/agent/panels/breakpoint_log_panel.h/.cc` | NEW — breakpoint event log |

**Complexity:** M-L | **Risk:** Medium (socket I/O timing, texture management)

---

## Cross-Cutting: Mesen2-OOS Window Embedding

### Feasibility Assessment

**Window embedding is NOT viable** without significant Mesen2 fork changes:
- Mesen2 uses Qt; yaze uses ImGui/SDL — incompatible event loops
- macOS removed window reparenting APIs in 10.7+
- No socket API for framebuffer sharing, render target exchange, or window handle passing

### Recommended Alternative: Control Panel + Screenshot Streaming

Instead of embedding, implement a **Mesen2 Control Panel** in yaze that provides:

1. **Launch/Connect:** Auto-discover running instances or launch `Mesen2 OOS.app` with ROM path
2. **Screenshot stream:** 10 FPS base64 PNG polling displayed in ImGui texture (Feature 10a)
3. **Game controls:** Pause/Resume/Step/Reset buttons via socket commands
4. **State panel:** SRAM viewer (Feature 2) + CPU state from existing MesenDebugPanel
5. **Bidirectional navigation:** Click room in yaze → warp in Mesen2; step in Mesen2 → update yaze state

This achieves 80% of the window-embedding UX without the platform-specific complexity.

### Future: Headless Mode (If Performance Needed)

If >30 FPS is needed, add to Mesen2-OOS fork:
- `GET_FRAMEBUFFER_BINARY` socket command → raw RGB565 buffer (eliminates PNG encode/decode overhead)
- Reduces per-frame latency from ~50ms to ~2ms
- Viable for 60 FPS in-editor preview
- **Effort:** ~500 lines C++ in Mesen2 + 200 lines yaze

---

## Execution Order & Dependencies

```
Phase 1 (Parallel — No Dependencies):
  Feature 1: Expanded Message Write     [S]  ← unblocks 6 Oracle tasks
  Feature 3: Write Conflict Warnings    [M]  ← safety for all editing
  Feature 4: Feature Flag Editor        [S-M]

Phase 2 (Parallel — No Dependencies):
  Feature 5: Track Overlay Enhancement  [M]
  Feature 6: Room Tag Editor Panel      [M]
  Feature 8: Dialogue Preview           [S]
  Feature 9: z3ed Batch Operations      [S]

Phase 3 (Depends on Mesen2 socket):
  Feature 2: SRAM Variable Viewer       [M]  ← needs socket connection
  Feature 10a: Screenshot Preview       [M]  ← needs socket connection
  Feature 10c: Room Warp Button         [S]  ← needs socket connection

Phase 4 (Depends on Phases 2+3):
  Feature 10b: SRAM Live Sync           [S]  ← wires Feature 2 to socket
  Feature 10d: Breakpoint Log           [M]  ← needs event subscription
  Feature 7: Progression Visualizer     [L]  ← lowest priority
```

**Total estimated new files:** 10-12
**Total estimated modified files:** 12-15
**Build target impact:** +10-12 new .cc files in CMakeLists

---

## Verification Strategy

After each feature:
1. `cmake --build build` → all targets clean, no new warnings
2. Launch yaze, open Oracle ROM (`oos168.sfc`)
3. Exercise the changed feature end-to-end
4. For socket features: verify with running Mesen2-OOS instance
5. For write features: verify ROM diff shows expected bytes changed
6. Switch between editors to verify no regressions

## Acceptance Criteria (Definition of Done)

These apply to every feature unless explicitly exempted:

- **User-visible behavior:** The feature can be exercised end-to-end from the GUI (or CLI for CLI-only items), not just via unit tests.
- **Safety:** Any ROM write path either:
  - only writes into non-protected regions, or
  - triggers a blocking warning popup listing conflicts and requires explicit "Save anyway".
- **Diffability:** A "before/after" ROM diff is feasible (at minimum: log the PC ranges written, counts, and any ownership conflicts).
- **Performance:** No new polling path exceeds 10 Hz by default unless the user opts in (socket screenshot streaming is opt-in).
- **Regression:** Feature does not break loading the Oracle ROM, saving a room, or opening the message editor.

## Agent Task Cards (Ready to Hand to Subagents)

Use this as the execution checklist. Each card is intended to be doable independently.

### Card A1: Expanded Message GUI Save Writes to Main ROM (Feature 1)

- Files: `src/app/editor/message/message_editor.cc`, `src/app/editor/message/message_editor.h`
- Work:
  - Make `SaveExpandedMessages()` write to `rom_` (not `expanded_message_bin_`), mirroring CLI `message-write`.
  - Ensure Ctrl+S persists both vanilla and expanded messages.
  - Display remaining bytes in expanded text region.
- Done when:
  - Save from GUI modifies the ROM data in-place and survives reload.
  - Capacity/overflow errors are surfaced to the user.

### Card B1: Save-Time Write Conflict Popup Is Blocking (Feature 3)

- Files: `src/app/editor/editor_manager.cc`, `src/app/editor/editor_manager.h`, `src/app/editor/ui/popup_manager.cc`, `src/app/editor/ui/popup_manager.h`
- Work:
  - Move from `LOG_WARN` to a blocking popup that lists conflicts and offers Save Anyway / Cancel.
  - Ensure "Save anyway" is one-shot and must be re-confirmed next time.
- Done when:
  - Attempting to save a room that touches a protected region reliably blocks with a popup.
  - Popup lists at least: PC address/range, owner/module, ownership type/policy.

### Card B2: Feature Flag Editor Panel (Feature 4)

- Files: `src/app/editor/agent/panels/feature_flag_editor_panel.*`, plus minimal HackManifest API if needed
- Work:
  - Display flags from manifest with current value/source.
  - Write changes back to `Config/feature_flags.asm` (atomic write, preserve comments).
  - Add warnings for flags tied to hooks that own ROM ranges.
- Done when:
  - A flag toggle updates `Config/feature_flags.asm` and is idempotent.
  - Panel never writes to the ROM directly.

### Card B3: Manifest Freshness UX (Cross-Cutting)

- Files: wherever HackManifest is loaded and stored globally (likely `src/app/controller.cc` or `src/app/editor/editor_manager.cc`) and a small UI surface (settings panel or a debug panel).
- Work:
  - Surface HackManifest provenance in the UI:
    - path of `hack_manifest.json`
    - file mtime and/or "loaded at" timestamp
    - a visible "Reload manifest" button that re-reads and re-indexes without restarting yaze
  - On reload: invalidate any cached ownership analyses so subsequent saves re-check conflicts.
  - Add a warning banner if the manifest file is missing/unreadable (safety fallback should become more conservative, not less).
- Done when:
  - A developer can confirm, from within yaze, exactly which manifest is being used.
  - Reload updates feature flags / protected regions / hooks without restart.

### Card B4: Protected Regions Inspector Panel (Cross-Cutting)

- Files: `src/app/editor/agent/panels/protected_regions_panel.h/.cc` (new), plus registration site (global panel registry or editor init).
- Work:
  - Show a searchable table of protected regions from HackManifest:
    - PC start/end, type/policy, owner/module/hook name, and any "reason" string if available
  - Provide convenience actions:
    - "Copy range"
    - "Find overlaps with current editor pending writes" (if available from Feature 3 plumbing)
  - Make the inspector read-only; it should educate and prevent foot-guns, not mutate ROM.
- Done when:
  - A developer can answer: "Why did yaze block this save?" without leaving the app.
  - Inspector matches the same conflict objects shown in the save-time warning popup (Feature 3).

### Card C1: Track Route Overlay + Stop Direction Arrows (Feature 5)

- Files: `src/app/editor/dungeon/dungeon_canvas_viewer.cc`, `src/app/editor/dungeon/dungeon_canvas_viewer.h`
- Work:
  - Implement route overlay (lines/arrows) on top of existing collision overlay.
  - Add stop tile arrows for $B7-$BA.
  - Add gap detection hints: object present but collision missing, and vice versa.
- Done when:
  - Visual output is deterministic and legible across multiple D6 rooms.

### Card C2: Room Tag Editor Panel (Feature 6)

- Files: `src/app/editor/dungeon/panels/room_tag_editor_panel.*`, `src/app/editor/dungeon/dungeon_editor_v2.cc`
- Work:
  - Show all tag slots + what rooms reference them.
  - "Assign to current room" is guarded (undoable and does not auto-save).
- Done when:
  - A user can discover free slots and assign tags without consulting external docs.

### Card C3: z3ed Batch Collision Generation (Feature 9)

- Files: `src/cli/handlers/game/dungeon_commands.cc`, `src/app/editor/dungeon/panels/minecart_track_editor_panel.cc`
- Work:
  - Multi-room CLI mode and "Generate All" GUI flow with dry-run summary then confirm.
- Done when:
  - Batch run reports per-room results and is safe by default.

### Card D1: SRAM Viewer Panel (Feature 2)

- Files: `src/app/editor/agent/panels/sram_viewer_panel.*`, manifest structs as needed
- Work:
  - Read SRAM blocks via socket at a sane rate; display named variables and bitfields.
  - Provide "poke" only behind a clear danger affordance (hold-to-confirm or explicit toggle).
- Done when:
  - Works against a running Mesen2-OOS instance with no crashes and stable refresh.

### Card D2: Screenshot Preview Panel (Feature 10a)

- Files: `src/app/editor/agent/panels/mesen_screenshot_panel.*`
- Work:
  - Opt-in polling, decode base64 PNG, upload texture, show overlay info.
- Done when:
  - Can pause/resume and adjust FPS; no runaway allocations.

## Risk Register / Known Pitfalls

- **Protected-region accuracy:** If manifest ownership is stale, yaze will either false-block or fail to block. Mitigation: add a visible "manifest loaded at" + "refresh" control.
- **Socket feature performance:** Screenshot PNG roundtrips are expensive. Mitigation: default low FPS and make it opt-in; consider raw framebuffer command only if needed.
- **Feature flag file format drift:** `Config/feature_flags.asm` must remain assembler-friendly. Mitigation: keep edits minimal and preserve comments/ordering.
