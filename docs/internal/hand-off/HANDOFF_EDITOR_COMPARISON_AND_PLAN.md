# Handoff: Editor Comparison and Plan Continuation

**Date:** 2026-01-31  
**Context:** yaze vs ZScreamDungeon/hmagic parity; ZScreamCLI validation  
**For:** Next agent to compare editors, identify differences, and continue the plan

---

## 1. What Was Done (This Session)

### Implemented

- **Dungeon save wired into Save ROM**  
  `EditorManager::SaveRom` in `src/app/editor/editor_manager.cc` now calls `current_editor_set->GetDungeonEditor()->Save()` after SaveDungeonMaps and before OverworldEditor::Save(). File > Save ROM persists dungeon objects, sprites, door pointers, and palettes.

- **Door marker and door pointer in Room**  
  - `Room::EncodeObjects` in `src/zelda3/dungeon/room.cc`: After the final layer terminator `0xFF 0xFF`, the stream now includes `0xF0 0xFF` and then each door via `door.EncodeBytes()` (per ZScreamDungeon / RoomDraw_DoorObject).  
  - `Room::SaveObjects` in `src/zelda3/dungeon/room.cc`: After writing encoded bytes, the room's door pointer is written to `doorPointers + (room_id_ * 3)` as a 3-byte SNES address pointing to the first byte after the marker.

- **Documentation**  
  - [docs/public/usage/zscream-validation.md](../../public/usage/zscream-validation.md): ZScreamCLI validation on macOS (golden ROMs, validate-yaze, features).  
  - [docs/public/usage/dungeon-editor.md](../../public/usage/dungeon-editor.md): Saving section updated; link to ZScream validation.  
  - [docs/internal/architecture/dungeon_editor_system.md](../architecture/dungeon_editor_system.md): Save flow and persistence coverage updated.

- **Build fix (macOS)**  
  `CMakeLists.txt`: On macOS (when not using llvm-brew toolchain), C++ stdlib headers (`<sdk>/usr/include/c++/v1`) are added with `include_directories(BEFORE SYSTEM ...)` so libc++'s `<math.h>` is found before `/usr/local/include`, fixing the `<cmath>`/isinf/signbit build failure. Full AI build: `cmake --preset mac-ai` then `cmake --build build_ai --preset mac-ai -j8`.

### Plan Reference

- Original plan: `~/.cursor/plans/yaze_zscream_parity_and_validation_fd5cb2f3.plan.md` (or workspace `.cursor/plans/` if moved).  
- Plan items 1–2 (wire dungeon save, door marker/pointer) are **done**. Items 3–5 (validate with ZScreamCLI, collision/torches/pits/blocks, optional dungeon layout parity) remain.

### Follow-up session (editor improvements)

- **Save flow**: `DungeonEditorV2::Save` now calls per room: `SaveObjects`, `SaveSprites`, `SaveRoomHeader`; then `SaveAllTorches`, `SaveAllPits`, `SaveAllBlocks`, `SaveAllChests`, `SaveAllPotItems` (with preserve/merge for unloaded rooms where applicable).
- **Room headers**: 14-byte header and message IDs written via `Room::SaveRoomHeader()`; message ID value loaded in `LoadRoomHeaderFromRom`.
- **Constants**: `kCustomCollisionRoomPointers`, `kCustomCollisionDataPosition`, `kCustomCollisionDataEnd` added for ZScream parity (no collision writer yet).
- **Validation**: [scripts/validate-yaze.sh](../../../scripts/validate-yaze.sh) added; [zscream-validation.md](../../public/usage/zscream-validation.md) and [dungeon-editor.md](../../public/usage/dungeon-editor.md) updated.

---

## 2. Editor Comparison: yaze vs ZScreamDungeon vs hmagic

### 2.1 High-Level Comparison

| Area | yaze | ZScreamDungeon | hmagic |
|------|------|----------------|--------|
| **Stack** | C++23, ImGui, CMake | C#, WPF/Avalonia, .NET | C, Windows MDI |
| **Dungeon** | DungeonEditorV2, Room model, SaveObjects/SaveSprites, door marker + pointer (done) | ZeldaFullEditor, Save.cs (SaveAllObjects, SaveAllSprites, SaveAllChests/Pots/Pits/Text) | DungeonFrame, DungeonLogic, DungeonMap |
| **Overworld** | OverworldEditor, 160 maps, compression, entrances/exits/items | OverworldMap, SaveAll | OverworldEdit, OverworldFrame, OverworldMap |
| **Graphics** | GraphicsEditor, SaveAllGraphicsData, GameData blocksets | GfxImportExport.SaveAllGfx | GraphicsLogic, GdiObjects |
| **Music** | MusicEditor, SPC parsing | — | TrackerLogic, AudioLogic (SPC700 replay in C) |
| **Text/Messages** | MessageEditor | SaveAllText | TextLogic, TextDlg |
| **Palette** | PaletteEditor, PaletteManager | — | PaletteFrame, PaletteSelector |
| **3D / Perspective** | — | — | PerspectiveLogic, PerspectiveDisplay (triforce, crystal) |
| **Patches/ASM** | Assembly editor, ASAR | PatchesSystem, AsmPlugin | PatchLogic, AsmPatchFrame |
| **Save ROM path** | SaveRom → DungeonEditor::Save, Overworld, Graphics, RomFileManager | Full save (objects, sprites, doors, chests, pots, pits, text, GFX) | Full save per editor |

### 2.2 Dungeon Persistence (Detail)

| Feature | yaze | ZScreamDungeon |
|---------|------|----------------|
| Objects (tile layers + terminator) | Yes (EncodeObjects, 0xFF 0xFF layers) | Yes (getTilesBytes) |
| Door marker 0xF0 0xFF + door list | Yes (added this session) | Yes |
| Door pointer table (kDoorPointers) | Yes (SaveObjects writes per room) | Yes (SaveObjectBytes, doorPos += 2) |
| Room object layout | In-place at existing room pointer | Repack into 5 fixed sections (DungeonSection1–5) |
| Sprites | SaveSprites() exists; called when Save ROM runs dungeon save | SaveAllSprites / SaveAllSprites2 |
| Room headers (message ID, collision, blockset, etc.) | Load only; no SaveRoomHeader | Written as part of save |
| Chests | Load only; DungeonEditorSystem stub | SaveAllChests |
| Pots | Load only | SaveAllPots |
| Pits | Load only | SaveAllPits |
| Collision | In header load; no dedicated save | Part of room/collision save |

### 2.3 Overworld / Graphics

- **yaze:** Overworld::Save (maps, compression, entrances, exits, items, overlays, music, area sizes). SaveAllGraphicsData + GameData blocksets.  
- **ZScreamDungeon:** Overworld and GFX save paths; ZScreamCLI has OverworldSaveWriter, GraphicsSaveWriter and RomComparer for overworld/graphics.  
- **hmagic:** Full overworld/graphics edit and save; reference only (no direct validation against it).

### 2.4 Gaps to Prioritize (for next agent)

1. **Room headers** – yaze does not write back message IDs, collision byte, blockset, etc. (RomComparer compares these). Either add SaveRoomHeader or document as known diff.  
2. **Collision / torches / pits / blocks** – yaze has LoadTorches, LoadBlocks, LoadPits and header collision; no Save* for these. ZScreamCLI validate-yaze --feature=collision|torches|pits|blocks will differ until yaze has save paths.  
3. **Chests / pots** – DungeonEditorSystem and UI exist; ROM persistence for chests/pots is not implemented.  
4. **Dungeon layout** – yaze uses in-place layout; ZScreamDungeon repacks into five sections. Byte-for-byte validate-yaze --feature=dungeon will still differ in layout; either add "semantic" comparison or document and keep in-place.  
5. **hmagic-only features** – Music tracker (SPC700 replay), 3D perspective editor: optional/later; not required for ZScream parity.

---

## 3. ZScreamCLI Validation (Recap)

- **Location:** `~/src/hobby/ZScreamCLI` (sibling to yaze).  
- **Commands:** `validate-yaze --yaze-rom=... --golden-rom=... --feature=dungeon|overworld|graphics|entrances|collision|torches|pits|blocks|all`  
- **Golden ROMs:** `./scripts/create-golden-roms.sh` or ZScreamDungeon; see ZScreamCLI `docs/GOLDEN_ROM_QUICK_START.md`, `docs/PLAN_FOLLOWUPS_2026-01-28.md` (base ROM fixture).  
- **yaze doc:** [docs/public/usage/zscream-validation.md](../../public/usage/zscream-validation.md).

---

## 4. Recommended Next Steps for Next Agent

1. **Compare editors concretely**  
   - For dungeon: diff yaze `Room::EncodeObjects`/SaveObjects vs ZScreamDungeon `Save.cs` (SaveAllObjects, SaveObjectBytes) and document any remaining format differences.  
   - For overworld/graphics: compare yaze Overworld::Save / SaveAllGraphicsData vs ZScreamCLI writers and RomComparer regions; list any constant/offset/order differences.

2. **Run validate-yaze**  
   - Generate golden ROMs (fix create-golden-roms base ROM if needed per ZScreamCLI PLAN_FOLLOWUPS).  
   - Build yaze (mac-ai: `cmake --preset mac-ai && cmake --build build_ai --preset mac-ai -j8`), load same base ROM, apply same edits, Save ROM.  
   - Run `validate-yaze --feature=dungeon` (then overworld, graphics). Record pass/fail and diff counts; fix or document.

3. **Continue plan items 3–5**  
   - **Item 3 (Validate with ZScreamCLI):** Automate or document the workflow; add CI or script that produces yaze ROM + validate-yaze for at least dungeon.  
   - **Item 4 (Collision / torches / pits / blocks):** Add Save* in yaze for the regions RomComparer uses; optionally run validate-yaze for those features.  
   - **Item 5 (Optional dungeon layout parity):** Only if byte-for-byte dungeon match is required: consider repack mode (write room data into five sections like ZScreamDungeon); else keep in-place and document.

4. **Optional: hmagic feature audit**  
   - List hmagic modules (TrackerLogic, PerspectiveLogic, etc.) and note which (if any) should be reflected in yaze roadmap; no need to implement in this handoff.

---

## 5. Key File References

| Purpose | Path |
|--------|------|
| Plan (original) | `~/.cursor/plans/yaze_zscream_parity_and_validation_fd5cb2f3.plan.md` |
| Save ROM flow | yaze `src/app/editor/editor_manager.cc` (SaveRom) |
| Dungeon save / door encoding | yaze `src/zelda3/dungeon/room.cc` (EncodeObjects, SaveObjects) |
| Dungeon architecture | yaze `docs/internal/architecture/dungeon_editor_system.md` |
| ZScream validation usage | yaze `docs/public/usage/zscream-validation.md` |
| ZScreamCLI validate-yaze | ZScreamCLI `src/ZScreamCLI/Commands/ValidateYazeCommand.cs` |
| ZScreamCLI RomComparer | ZScreamCLI `src/ZScreamCore/Validation/RomComparer.cs` |
| ZScreamDungeon Save | ZScreamDungeon `ZeldaFullEditor/Save.cs` |
| hmagic structs | hmagic `structs.h` |

---

## 6. Summary

- **Done:** Dungeon save in Save ROM; door marker `0xF0 0xFF` and door pointer table in yaze; ZScream validation doc; macOS C++ stdlib build fix; full AI build (mac-ai) succeeds.  
- **Next agent:** Compare yaze vs ZScreamDungeon/hmagic editors (tables above + code diff), run validate-yaze, then continue plan items 3–5 (validation workflow, collision/torches/pits/blocks save, optional dungeon repack).
