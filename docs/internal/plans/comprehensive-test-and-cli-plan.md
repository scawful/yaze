# Comprehensive Plan: E2E Testing & z3ed CLI Improvements

This document outlines the design for ROM-dependent End-to-End (E2E) tests for `yaze` editors and improvements to the `z3ed` CLI tool.

## 1. E2E Testing Design

The goal is to ensure stability and correctness of major editors by simulating user actions and verifying the state of the ROM and application.

### 1.1. Test Infrastructure

We will extend the existing `RomDependentTestSuite` in `src/app/test/rom_dependent_test_suite.h` or create a new `EditorE2ETestSuite`.

**Key Components:**
- **Test Fixture:** A setup that loads a clean ROM (or a specific test ROM) before each test.
- **Action Simulator:** Helper functions to simulate editor actions (e.g., `SelectTile`, `PlaceObject`, `Save`).
- **State Verifier:** Helper functions to verify the ROM state matches expectations after actions.

### 1.2. Overworld Editor Tests

**Scope:**
- **Draw Tile 16:** Select a tile from the blockset and place it on the map. Verify the map data reflects the change.
- **Edit Tile 16:** Modify a Tile16 definition (e.g., change sub-tiles, flip, priority). Verify the Tile16 data is updated.
- **Multi-select & Draw:** Select a range of tiles and draw them. Verify all tiles are placed correctly.
- **Manipulate Entities:** Add, move, and delete sprites/exits/items. Verify their coordinates and properties.
- **Change Properties:** Modify map properties (e.g., palette, music, message ID).
- **Expanded Features:** Test features specific to ZSCustomOverworld (if applicable).

**Implementation Strategy:**
- Use `OverworldEditor` class directly or via a wrapper to invoke methods like `SetTile`, `UpdateEntity`.
- Verify by inspecting `OverworldMap` data and `GameData`.

### 1.3. Dungeon Editor Tests

**Scope:**
- **Object Manipulation:** Add, delete, move, and resize objects.
- **Door Manipulation:** Change door types, toggle open/closed states.
- **Items & Sprites:** Add/remove items (chests, pots) and sprites. Verify properties.
- **Room Properties:** Change header settings (music, palette, floor).

**Implementation Strategy:**
- Use `DungeonEditor` and `Room` classes.
- Simulate mouse/keyboard inputs if possible, or call logical methods directly.
- Verify by reloading the room and checking object lists.

### 1.4. Graphics & Palette Editor Tests

**Scope:**
- **Graphics:** Edit pixel data in a sprite sheet. Save and reload. Verify pixels are preserved and no corruption occurs.
- **Palette:** Change color values. Save and reload. Verify colors are preserved.

**Implementation Strategy:**
- Modify `Bitmap` data directly or via editor methods.
- Trigger `SaveGraphics` / `SavePalettes`.
- Reload and compare byte-for-byte.

### 1.5. Message Editor Tests

**Scope:**
- **Text Editing:** Modify dialogue text.
- **Command Insertion:** Insert control codes (e.g., wait, speed change).
- **Save/Reload:** Verify text and commands are preserved.

**Implementation Strategy:**
- Use `MessageEditor` backend.
- Verify encoded text data in ROM.

## 2. z3ed CLI Improvements

The `z3ed` CLI is a vital tool for ROM hacking workflows. We will improve its "doctor" and "comparison" capabilities and general UX.

### 2.1. Doctor Command Improvements (`z3ed doctor`)

**Goal:** Provide deeper insights into ROM health and potential issues.

**New Features:**
- **Deep Scan:** Analyze all rooms, not just a sample (already exists via `--all`, but make it default or more prominent).
- **Corruption Heuristics:** Check for common corruption patterns (e.g., invalid pointers, overlapping data).
- **Expanded Feature Validation:** Verify integrity of expanded tables (Tile16, Tile32, Monologue).
- **Fix Suggestions:** Where possible, offer specific commands or actions to fix issues (e.g., "Run `z3ed fix-checksum`").
- **JSON Output:** Ensure comprehensive JSON output for CI/CD integration.

### 2.2. Comparison Command Improvements (`z3ed compare`)

**Goal:** Make it easier to track changes and spot unintended regressions.

**New Features:**
- **Smart Diff:** Ignore insignificant changes (e.g., checksum, timestamp) if requested.
- **Visual Diff (Text):** Improved hex dump visualization of differences (side-by-side).
- **Region Filtering:** Allow comparing only specific regions (e.g., "Overworld", "Dungeon", "Code").
- **Summary Mode:** Show high-level summary of changed areas (e.g., "3 Rooms modified, 1 Overworld map modified").

### 2.3. UX Improvements

- **Progress Bars:** Add progress indicators for long-running operations (like full ROM scan).
- **Interactive Mode:** For `doctor`, allow interactive fixing of simple issues.
- **Better Help:** Improve help messages and examples.
- **Colorized Output:** Enhance readability with color-coded status (Green=OK, Yellow=Warning, Red=Error).

## 3. Implementation Roadmap

1.  **Phase 1: Foundation & CLI**
    - Refactor `z3ed` commands for better extensibility.
    - Implement improved `doctor` and `compare` logic.
    - Add UX enhancements (colors, progress).

2.  **Phase 2: Editor Test Framework**
    - Create `EditorTestBase` class.
    - Implement helpers for ROM loading/resetting.

3.  **Phase 3: Specific Editor Tests**
    - Implement Overworld tests.
    - Implement Dungeon tests.
    - Implement Graphics/Palette/Message tests.

4.  **Phase 4: Documentation & Integration**
    - Update docs with usage guides.
    - Integrate into CI pipeline.

## 4. Verification Plan

- **Unit Tests:** Verify individual command logic.
- **Integration Tests:** Run `z3ed` against known good and bad ROMs.
- **Manual Verification:** Use the improved CLI tools on real projects to verify utility.
