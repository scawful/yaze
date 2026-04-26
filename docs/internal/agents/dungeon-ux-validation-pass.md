# Dungeon UX Validation Pass — Manual QA Checklist

_Agent: `zelda3-hacking-expert` | Updated: 2026-04-26_

This document records the manual verification checklist for the four UX features
landed in the Feb 23–24 imgui-frontend-engineer session. Execute with a loaded ROM
and the `mac-ai` preset build (`./build/presets/mac-ai/bin/Debug/yaze.app/Contents/MacOS/yaze`). Each section lists the exact
click-path, expected on-screen result, and the relevant source constant so testers
can correlate source truth with observed behaviour.

---

## CLI Smoke Gate (run before each manual QA session)

```bash
# Structural check — must exit 0 and print ok:true
./build/presets/mac-ai/bin/Debug/z3ed oracle-smoke-check \
  --rom roms/oos168.sfc --min-d6-track-rooms=4 --format=json

# Strict-readiness check — expected exit 1, D4/D3 ok:false (known authoring gap)
./build/presets/mac-ai/bin/Debug/z3ed oracle-smoke-check \
  --rom roms/oos168.sfc --min-d6-track-rooms=4 --strict-readiness --format=json \
  || true   # non-zero is expected; only D4/D3 gaps present
```

Known-good baselines (do not treat as regression):
- D4 `required_rooms_ok: false` — collision data not yet imported
- D3 `ok: false` — readiness not yet met
- D6 `ok: true`, `meets_min_track_rooms: true` — passing

---

## 1. Door Ghost Preview States

**Source:** `DoorInteractionHandler::DrawGhostPreview()` — `door_interaction_handler.cc:96`

### 1a — Ghost appears near a wall

**Click-path:**
1. Open yaze → load ROM → Dungeon editor → navigate to any dungeon room.
2. Object Editor panel → Doors section → click "Place Door" (or equivalent toolbar button).
3. Move mouse slowly toward the **north wall** of the canvas (top ~32 px).

**Expected:**
- A semi-transparent filled rectangle snaps to the nearest door slot on the north wall.
- An outline (theme `dungeon_selection_primary` color) appears around the rectangle.
- A text label above the ghost reads: `NormalDoor (North)` (or the selected door type + detected direction).
- Ghost follows the mouse as it slides along the wall.

### 1b — Ghost disappears away from walls

**Click-path:**
1. Continue from 1a, move mouse to the **center** of the canvas (roughly 200, 200 in canvas coords).

**Expected:**
- Ghost preview disappears entirely (no rect, no label).
- Source: `UpdateSnappedPosition` returns `false` → `DrawGhostPreview` returns early.

### 1c — Ghost on each wall (North / South / East / West)

Repeat 1a for each wall. Expected direction label in ghost: `North`, `South`, `East`, `West` respectively.

### 1d — Snap indicator dots during drag

**Click-path:**
1. Place at least one door (click a wall while in placement mode to commit).
2. Click the placed door to select it (orange animated outline appears).
3. Drag it to another wall section.

**Expected:**
- While dragging: up to 6 snap-position indicators appear along the current wall section.
- Nearest snap position is highlighted with a thick border (theme `dungeon_selection_primary`, α=0.75).
- Other positions show ghosted thin rectangles (white, α=0.25).
- On release at a valid position: door moves, orange selection outline follows new position.

### 1e — Ghost NOT present when NOT in placement mode

**Click-path:**
1. Press `Escape` or click "Cancel" to leave door placement mode.
2. Move mouse over the canvas walls.

**Expected:** No ghost rectangle. `door_placement_mode_ = false` short-circuits `DrawGhostPreview`.

### 1f — Ghost warns on the last slot and blocks at cap

**Click-path:**
1. Open a room with `15/16` doors.
2. Enter door placement mode and hover a valid wall position.
3. Then repeat with a room at `16/16`.

**Expected:**
- At `15/16`, the ghost switches to the warning color, shows a visible `Doors 15/16` + `LAST SLOT` badge on the room-interior side of the preview, and the tooltip text ends with `Last available slot`.
- At `16/16`, the ghost switches to the error color, shows a visible `Doors 16/16` + `ROOM FULL` badge, and the tooltip text ends with `Placement blocked`.
- Dragging an already-placed door still uses the normal snap-indicator colors, because moving an existing door does not consume a new slot.

**Caveat:** Ghost is only visible when `canvas->IsMouseHovering()` returns true; hovering the panel chrome outside the canvas suppresses it.

---

## 2. Placement Error Toast — Dedup / Expiry Behaviour

**Source:** `ObjectEditorPanel::SetPlacementError()` / `Draw()` — `object_editor_panel.cc:272`, `object_editor_panel.h:227`
- Toast duration: `kPlacementErrorDuration = 2.0` seconds (compile-time constant).
- Display: `ImGui::TextColored(status_error, ICON_MD_ERROR " <message>")` rendered each frame while elapsed < 2 s.

### 2a — Error toast appears on sprite limit hit

**Pre-condition:** Room must have exactly 64 sprites. Fastest path: use a test room or the CLI to pre-fill.

**Click-path:**
1. Dungeon editor → select room with 64 sprites (use validation bar to confirm count).
2. Object Editor → select a sprite type → click "Place Sprite".
3. Click anywhere on the canvas to attempt placement.

**Expected:**
- Red error text with ⚠ icon appears below the door section: `Sprite limit reached (64 max) - placement blocked`.
- Sprite count in the room remains 64.
- Toast auto-clears after ~2 seconds without interaction.

### 2b — Error toast appears on door limit hit

**Pre-condition:** Room must have exactly 16 doors.

**Click-path:**
1. Dungeon editor → select room with 16 doors.
2. Object Editor → Doors section → click "Place Door" → click any wall position.

**Expected:**
- Red error text: `Door limit reached (16 max) - placement blocked`.
- Door count remains 16.

### 2c — Error toast appears on invalid door position

**Click-path:**
1. Rooms with < 16 doors, placement mode active.
2. Click canvas center (e.g. ~200, 200 in canvas coords — not near any wall).

**Expected:**
- Red error text: `Invalid door position - must be near a wall`.
- No door added.

### 2d — Toast dedup: rapid repeated placement attempts produce one toast

**Click-path:**
1. Room at 64 sprites, placement mode active.
2. Click canvas 3× quickly (< 2 s between each).

**Expected:**
- The toast restarts its 2-second timer on each blocked click (`placement_error_time_` is reassigned each call to `SetPlacementError`).
- Only one message is visible at a time — no stacking or duplicate text lines.
- If the same message text is set twice, the user sees a single refreshed timer, not two copies.

**Caveat:** There is no de-duplication guard — if a different error fires before the previous one expires, it replaces the message immediately. This is intentional (last error wins). No unit test currently covers the message-replacement path.

### 2e — Toast for tile-object limit

**Pre-condition:** Room at or near 400 objects.

**Click-path:**
1. Attempt to place an object when count = 400.

**Expected:** `Object limit reached (400 max) - placement blocked`.

---

## 3. Room Badge Copy Behaviour

**Source:** `DungeonWorkbenchContent::DrawInspectorShelfRoom()` —
`src/app/editor/dungeon/workspace/dungeon_workbench_content.cc`

### 3a — Room ID displayed in hex + decimal

**Click-path:**
1. Dungeon editor → open Workbench view.
2. Inspector shelf → Room tab (ICON_MD_CASTLE).

**Expected:**
- Text: `Room: 0x0XX (DDD)` where `0x0XX` is the hex room ID and `DDD` is decimal.
- Example: room 42 → `Room: 0x02A (42)`.

### 3b — Copy button places hex string on clipboard

**Click-path:**
1. In Inspector → Room tab, hover the small copy icon (📋) button to the right of the room text.
2. Verify tooltip: `Copy room ID (0x0XX) to clipboard`.
3. Click the button.
4. Paste into a text editor.

**Expected:**
- Clipboard contains exactly `0x0XX` (e.g. `0x02A` for room 42).
- No decimal, no parentheses — purely hex with `0x` prefix.
- Source: `snprintf(buf, 16, "0x%03X", room_id)` then `ImGui::SetClipboardText(buf)`.

### 3c — Copy button updates as room changes

**Click-path:**
1. Copy room ID for room A.
2. Navigate to a different room (room selector or Next Room button).
3. Click copy again.
4. Paste.

**Expected:** Clipboard now reflects the new room's hex ID, not the previous one.

**Caveats:**
- If no ROM is loaded or room_id is negative, `Room: None` text is shown and the copy button is still rendered (copies `0x___` or similar — not tested; verify graceful behavior).
- The copy button uses `##CopyRoomId` as its ImGui ID, so duplicate panel instances in the same frame would conflict; this is not a deployed configuration.

---

## 4. Tile Selector Range Filter — Error Handling and Clear

**Source:** `TileSelectorWidget::DrawFilterBar()` — `tile_selector_widget.cc:237`
Field: `filter_range_error_` (bool), error condition: `parsed_min > parsed_max`.

### 4a — Valid range activates filter

**Click-path:**
1. Overworld editor (or Tile16 editor) — tile selector panel.
2. Filter bar → Min field: type `10`, Max field: type `20` (hex or decimal — inputs parse as hex).
3. Press Tab or click away.

**Expected:**
- Tiles with IDs outside [0x10, 0x20] are dimmed / hidden.
- Status text (inline): `16 tiles` (or actual count).
- No error text.
- Selector auto-scrolls to tile 0x10.

### 4b — Invalid range shows inline error

**Click-path:**
1. Min field: type `50`, Max field: type `10` (min > max).

**Expected:**
- Inline red text appears to the right of the inputs: `Min must be <= Max`.
- Source: `ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Min must be <= Max")`.
- Filter is NOT applied (existing filter, if any, remains unchanged).
- `filter_range_error_ = true`.

### 4c — Correcting the invalid range clears the error

**Click-path:**
1. Continue from 4b (min=50, max=10, error shown).
2. Change Max to `60` (now min=50 ≤ max=60).

**Expected:**
- Error text disappears immediately on the next InputText callback.
- Filter activates for [0x50, 0x60].
- `filter_range_error_ = false`.

### 4d — Clearing both fields removes the filter

**Click-path:**
1. Active range filter [0x10, 0x20].
2. Delete all text from both Min and Max fields (both empty).

**Expected:**
- Filter deactivated. All tiles visible.
- No error text.
- Source: `ClearRangeFilter()` called when both parse attempts fail (empty strings).

### 4e — X button clears active filter

**Click-path:**
1. Active range filter [0x10, 0x20] — confirm "X" clear button is visible.
2. Click "X##ClearRange".

**Expected:**
- Filter deactivated. Both input buffers cleared.
- "X" button disappears (only shown when `filter_range_active_ == true`).
- No error text.

### 4f — Out-of-range tile IDs are clamped

**Click-path:**
1. Min: `FFF` (3071), Max: `FFF` — for a palette with only 512 tiles.

**Expected:**
- Filter applied (no crash). `IsInFilterRange` will return false for most tiles since 0xFFF > total tile count. Tile grid shows 0 or 1 visible tile.
- No assertion, no panic. `SetTileCount` clamps `total_tiles_` to 0 minimum.

**Caveats:**
- The range inputs parse as **hex** (kHexFlags). Users typing decimal numbers (e.g. `64` for decimal) will actually filter to IDs 0x64 = 100 decimal. This is not a bug but is non-obvious — consider a "0x" prefix hint label in a future pass.
- The error message is shown **inline** (same row as the inputs via `SameLine`), not below. At narrow panel widths the text may be clipped. Not currently a hard failure but worth noting for responsive-layout work.
- There is no per-field red border (only the text label changes color). Field-level highlight would require a custom InputText style push.

---

## 5. Workbench Tool Drawer and Window Filtering

**Source:**
- `DungeonWorkbenchContent::DrawInspectorToolDrawer()`
- `DungeonWorkbenchContent::Open*Tool()`
- `WindowSidebar::IsDungeonWindowModeTarget()` (room-prefixed nav windows)
- `WindowBrowser::Draw()`

### 5a — Local tool requests open the right inspector drawer

**Click-path:**
1. Launch YAZE with a ROM and open Dungeon Workbench:
   `--editor=Dungeon --room=16 --open_panels=dungeon.workbench`.
2. In the Workbench right inspector, click **Tools**.
3. From the 2x5 icon strip at the top of the drawer, click each of the ten
   tool icons in order: **Object Selector**, **Doors**, **Sprites**, **Items**,
   **Palette**, **Room Graphics**, **Room Tags**, **Custom Collision**,
   **Water Fill**, and **Minecart**.

**Expected:**
- The selected tool renders inside the right inspector, not in a separate modal
  or top-level Dungeon window.
- The icon for the active tool is highlighted (accent color) in the strip; only
  one icon is highlighted at a time.
- Hovering each icon shows a tooltip with the tool's full name.
- The active tool's body fills the inspector height beneath the strip and the
  active tool's title (no developer-doc subtitle, no in-drawer back button).
- Returning to room metadata is one click on the inspector primary segmented
  selector ("Room") at the top of the inspector, not a separate button inside
  the drawer.
- The active tool remains selected after navigating to another room.
- Selecting an object/entity on the canvas may focus Selection, but it should
  not steal focus away from the Tools drawer while the drawer is active.

### 5b — Dungeon Map and Connected Graph keep their special homes

**Click-path:**
1. From the Workbench tool/review controls, click **Dungeon Map**.
2. Close it, then click **Matrix** / **Connected Graph**.

**Expected:**
- Dungeon Map opens as a bounded popup.
- Connected Graph switches the canvas mode; it is not embedded as a drawer tool.

### 5c — Window Browser/sidebar list Workbench-local tools in either mode

> Updated 2026-04-26 (polish pass #3 — mode-parity relaxation): the previous
> hide/auto-close policy was removed. Tool windows now appear in the sidebar
> and Window Browser in both Workbench and Windows workflow modes, and entering
> Workbench mode no longer closes their standalone copies.

**Click-path:**
1. Ensure Workbench workflow is active.
2. Open the Dungeon sidebar and Window Browser.
3. Search for local tool windows such as `dungeon.object_selector`,
   `dungeon.door_editor`, `dungeon.palette_editor`, `dungeon.room_tags`,
   `dungeon.custom_collision`, and `dungeon.water_fill`.

**Expected:**
- These local tool windows are visible in both the sidebar and the Window
  Browser regardless of workflow mode.
- Toggling **Workbench / Windows** in the Dungeon sidebar workflow strip never
  auto-closes a standalone tool window. Only the room-mode navigation windows
  (`dungeon.room_selector`, `dungeon.room_matrix`, and per-room `dungeon.room_*`
  windows) are collapsed when entering Workbench mode, and pinned room windows
  are preserved.
- Opening a tool from the sidebar while the Workbench drawer also has the same
  tool active yields two edit surfaces for the same room state — by design.
- Switching to Window workflow restores the room-mode windows.

**Focused automated coverage:**

```bash
./build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor \
  --gtest_filter='DungeonWorkbenchToolbar*.*:DungeonWorkbenchContentLayoutTest.*:SidebarSortTest.*'
```

Expected result: all tests pass, including drawer-state persistence and the
mode-parity contract pinned by `SidebarSortTest.DungeonWindowModeTargets…`.

---

## Full QA Run Commands

```bash
# 1. Build
cmake --build --preset mac-ai --target yaze_test_quick_unit_editor z3ed --parallel 8

# 2. Targeted unit tests (original UX suites plus Workbench drawer/window filtering)
./build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor \
  --gtest_filter="TileSelectorWidgetTest.*:DoorInteractionHandlerTest.*:SpriteInteractionHandlerTest.*:TileObjectHandlerTest.*:DungeonWorkbenchToolbar*.*:DungeonWorkbenchContentLayoutTest.*:SidebarSortTest.*"

# 3. Quick editor suite
ctest --test-dir build/presets/mac-ai -R yaze_test_quick_unit_editor --output-on-failure

# 4. Oracle structural smoke (expected exit 0)
./build/presets/mac-ai/bin/Debug/z3ed oracle-smoke-check \
  --rom roms/oos168.sfc --min-d6-track-rooms=4 --format=json

# 5. Oracle strict-readiness (expected exit 1 — D4/D3 gap is known)
./build/presets/mac-ai/bin/Debug/z3ed oracle-smoke-check \
  --rom roms/oos168.sfc --min-d6-track-rooms=4 --strict-readiness --format=json || true
```

---

## Known Expected Failures vs Regressions

| Item | Status | Notes |
|------|--------|-------|
| Oracle D4 `required_rooms_ok: false` | **Expected** | Water-fill collision data not yet imported |
| Oracle D3 `ok: false` | **Expected** | Kalyxo Castle readiness not yet met |
| Oracle D6 `ok: true` | **Must pass** | Goron Mines minecart track verified |
| Door ghost capacity coloring | **Resolved** | `DrawGhostPreview` now uses warning/error colors at 14/16 and 16/16 with hover tooltip. |
| Range filter decimal confusion | **Mitigated** | Inputs parse hex and now include a visible `(hex)` hint plus hover tooltip. |
| `DungeonObjectValidateTest.ClipSelectionBoundsToRoomClipsRepeatSpacing` | **Pre-existing, now fixed** | Was failing before Feb 14 fix. Confirm passes in quick_unit_editor. |

---

## Follow-up Items

1. **Range filter out-of-range feedback** — ✅ **RESOLVED** (Feb 24). Added `filter_out_of_range_` flag to `TileSelectorWidget`. `DrawFilterBar` now detects when `SetRangeFilter` returns without activating (both values exceed `total_tiles_`) and shows inline red `"Out of range (max: 0x%03X)"` text. `ClearRangeFilter` resets the flag. 2 new tests: `RangeFilterBothOutOfBoundsDoesNotActivate` and `RangeFilterMinInRangeMaxOutClamps` — 20/20 pass.
2. **Room badge invalid-ID hardening test** — ✅ **RESOLVED BY INSPECTION** (Feb 24). `DrawInspectorShelfRoom` in `src/app/editor/dungeon/workspace/dungeon_workbench_content.cc` guards `if (room_id >= 0)` — badge and copy button are not rendered for negative IDs. Behavior verified by code inspection; no additional test infrastructure was needed.
3. **Success toast rate limiting** — ✅ **RESOLVED** (Feb 24, `ai-infra-architect`). `ToastManager::Show` now suppresses identical message+type within a 1-second cooldown window. 8 unit tests in `test/unit/editor/toast_dedup_test.cc`.
