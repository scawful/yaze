# v0.5.4 Implementation Plan

**Created:** 2026-01-20
**Status:** Planning
**Target:** February 2026

## Executive Summary

v0.5.4 focuses on editor feature completion, music serialization, and workspace management improvements. This plan identifies concrete tasks, dependencies, and suggested agent assignments.

## Priority 1: Editor Feature Completion

### 1.1 Palette JSON Import/Export

**Files:**
- `src/app/editor/palette/palette_group_panel.cc:529` (ExportToJson)
- `src/app/editor/palette/palette_group_panel.cc:534` (ImportFromJson)

**Implementation:**
```cpp
// JSON schema for palette export
{
  "version": 1,
  "group": "overworld_main",
  "palettes": [
    {
      "index": 0,
      "colors": ["$7FFF", "$0000", ...]  // SNES format
    }
  ]
}
```

**Dependencies:**
- nlohmann/json (already integrated)
- Existing `gfx::SnesPalette` serialization

**Agent Assignment:** Codex GPT-5.2 (structured JSON schema)

**Tests to Add:**
- `test/unit/palette_json_test.cc`
- Round-trip export/import verification

### 1.2 Palette Clipboard Import

**File:** `src/app/editor/palette/palette_group_panel.cc:559`

**Implementation:**
- Parse comma-separated SNES colors from clipboard
- Support formats: `$7FFF,$0000,...` and `7FFF,0000,...`
- Validate color count matches palette size

**Dependencies:** None (uses existing ImGui clipboard)

### 1.3 Graphics Screen Editor Operations

**File:** `src/app/editor/graphics/screen_editor.h:46-52`

**Operations to implement:**
- `Undo()` / `Redo()` - Snapshot-based (see undo_redo_system.md)
- `Cut()` / `Copy()` / `Paste()` - Selection-based tile operations
- `Find()` - Tile pattern search
- `Save()` - Persist changes to ROM

**Dependencies:**
- Undo system from DungeonObjectEditor (can adapt pattern)
- Tile selection system

### 1.4 Link Sprite Reset to Vanilla

**File:** `src/app/editor/graphics/link_sprite_panel.cc:339`

**Implementation:**
- Store vanilla Link sprite data on first load
- Provide "Reset to Vanilla" button
- Confirm dialog before reset

## Priority 2: Music Editor Serialization

### 2.1 SaveInstruments

**File:** `src/zelda3/music/music_bank.cc:925`

**Implementation:**
- Serialize instrument ADSR envelopes
- Write instrument data to ROM
- Update instrument pointer table

**Agent Assignment:** AFS Din (audio/music domain)

### 2.2 SaveSamples (BRR Encoding)

**File:** `src/zelda3/music/music_bank.cc:996`

**Implementation:**
- BRR (Bit Rate Reduction) encoding algorithm
- Sample loop point handling
- ARAM allocation management

**Complexity:** High - requires DSP knowledge

**Research Resources:**
- SPC700 documentation
- Existing BRR decoders in codebase

### 2.3 WAV/BRR Integration

**File:** `src/zelda3/music/music_bank.cc:400`

**Implementation:**
- WAV file parsing (16-bit PCM)
- Sample rate conversion to 32kHz
- BRR encoding pipeline

## Priority 3: Workspace & Layout

### 3.1 Layout Serialization

**Files:**
- `src/app/editor/ui/workspace_manager.cc:18` (Save)
- `src/app/editor/ui/workspace_manager.cc:26` (Load)
- `src/app/editor/ui/workspace_manager.cc:34` (Reset)

**Implementation:**
```cpp
// Use ImGui's built-in INI serialization
ImGui::SaveIniSettingsToDisk(path);
ImGui::LoadIniSettingsFromDisk(path);

// Custom layout metadata in JSON sidecar
{
  "name": "my_layout",
  "created": "2026-01-20",
  "panels": ["overworld", "dungeon", "palette"]
}
```

### 3.2 Preset Layouts

**Files:**
- `src/app/editor/ui/workspace_manager.cc:129` (Developer)
- `src/app/editor/ui/workspace_manager.cc:136` (Designer)
- `src/app/editor/ui/workspace_manager.cc:143` (Modder)

**Presets:**

| Preset | Panels |
|--------|--------|
| Developer | All debug tools, console, memory viewer |
| Designer | Graphics, Palette, Sprite, Screen |
| Modder | Overworld, Dungeon, Hex, Assembly |

### 3.3 Window Cycling

**Files:**
- `src/app/editor/ui/workspace_manager.cc:242` (Next)
- `src/app/editor/ui/workspace_manager.cc:246` (Previous)

**Implementation:**
- Track window focus order
- Cycle through docked windows with Ctrl+Tab
- Visual indicator during cycling

## Priority 4: Platform & Performance

### 4.1 Shutdown Performance

**File:** `src/app/platform/window.cc:168`

**Issue:** Graphics arena shutdown order causes slow exit

**Fix:**
- Clear texture queue before arena shutdown
- Release GPU resources in correct order
- Add timeout for stuck operations

### 4.2 CRC32 Calculation

**Files:**
- `src/core/asar_wrapper.cc:330` (library path)
- `src/core/asar_wrapper.cc:501` (CLI path)

**Implementation:**
- Use standard CRC32 algorithm
- Cache results for unchanged files

### 4.3 ZScream Format Parsing

**Files:**
- `src/core/project.cc:694`
- `src/core/project.cc:871`

**Status:** Blocked - awaiting format specification

**Action:** Document known ZScream format fields from reverse engineering

## Test Coverage Additions

| Feature | Test File | Type |
|---------|-----------|------|
| Palette JSON | `test/unit/palette_json_test.cc` | Unit |
| Layout serialization | `test/integration/workspace_test.cc` | Integration |
| Music save | `test/integration/music_bank_test.cc` | Integration |
| BRR encoding | `test/unit/brr_codec_test.cc` | Unit |

## Agent Assignments Summary

| Task | Agent | Rationale |
|------|-------|-----------|
| Palette JSON | Codex GPT-5.2 | Structured data, JSON schema |
| BRR Encoding | AFS Din | Audio codec expertise |
| Layout System | Claude | ImGui internal API |
| Screen Editor | Gemini | Visual reasoning |
| CRC32 | Any | Algorithmic, well-documented |

## Timeline

| Week | Focus |
|------|-------|
| 1 | Palette JSON, Clipboard import |
| 2 | Layout serialization, Presets |
| 3 | Music SaveInstruments |
| 4 | Screen Editor operations |
| 5 | BRR encoding (stretch) |
| 6 | Testing, polish, release |

## Success Criteria

- [ ] Palette JSON round-trip works
- [ ] Clipboard import parses SNES colors
- [ ] Layout save/load persists docking state
- [ ] 3 preset layouts functional
- [ ] Music instruments save to ROM
- [ ] All new tests passing

## Open Questions

1. Should palette JSON include RGB values for compatibility with other tools?
2. BRR encoding quality settings (compression ratio vs. fidelity)?
3. Should presets be user-editable or hardcoded?

## Related Documents

- [roadmap.md](../roadmap.md) - Overall project roadmap
- [undo_redo_system.md](../architecture/undo_redo_system.md) - Undo system design
- [editor_card_layout_system.md](../architecture/editor_card_layout_system.md) - Layout architecture
