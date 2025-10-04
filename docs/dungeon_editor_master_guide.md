# Yaze Dungeon Editor: Master Guide

**Last Updated**: October 4, 2025

This document provides a comprehensive, up-to-date overview of the Yaze Dungeon Editor, consolidating all recent design plans, testing results, and critical fixes. It serves as the single source of truth for the editor's architecture, data structures, and future development.

## 1. Current Status: Production Ready

After a significant refactoring and bug-fixing effort, the Dungeon Editor's core functionality is **complete and stable**.

- **Core Logic**: The most complex features, including the 3-type object encoding/decoding system and saving objects back to the ROM, are **fully implemented**.
- **Testing**: The test suite is now **100% stable**, with all 29 unit, integration, and E2E tests passing. Critical `SIGBUS` and `SIGSEGV` crashes have been resolved by replacing the unstable `MockRom` with a real ROM file for testing.
- **Rendering**: The rendering pipeline is verified, correct, and performant, properly using the graphics arena and a component-based architecture.
- **Coordinate System**: A critical object positioning bug has been fixed, ensuring all objects render in their correct locations.

### Known Issues & Next Steps

While the core is stable, several UI and performance items remain:

1.  **UI Polish (High Priority)**:
    *   Implement human-readable labels for rooms and entrances in selection lists.
    *   Add tab management features (`+` to add, `x` to close) for a better multi-room workflow.
2.  **Performance (Medium Priority)**:
    *   Address the slow initial load time (~2.6 seconds) by implementing lazy loading for rooms.
3.  **Palette System (Medium Priority)**:
    *   Fix the handling of palette IDs greater than 19 to prevent fallbacks to palette 0.

## 2. Architecture: A Component-Based Design

The editor was refactored into a modern, component-based architecture, reducing the main editor's complexity by **79%**. The `DungeonEditorV2` class now acts as a thin coordinator, delegating all work to specialized components.

### Core Components

-   **`DungeonEditorV2`**: The main orchestrator. Manages the 3-column layout and coordinates the other components.
-   **`DungeonRoomLoader`**: Handles all data loading from the ROM, now optimized with parallel processing.
-   **`DungeonRoomSelector`**: Manages the UI for selecting rooms and entrances.
-   **`DungeonCanvasViewer`**: Responsible for rendering the room, objects, and sprites onto the main canvas.
-   **`DungeonObjectSelector`**: Provides the UI for browsing and selecting objects, sprites, and other editable elements.
-   **`DungeonObjectInteraction`**: Manages all mouse input, selection, and drag-and-drop on the canvas.
-   **`ObjectRenderer`**: A high-performance system for rendering individual dungeon objects, featuring a graphics cache.

### Data Flow

1.  **Load**: `DungeonEditorV2::Load()` calls `DungeonRoomLoader` to load all room data from the ROM.
2.  **Update**: The editor's `Update()` method calls `Draw()` on each of the three main UI components (`RoomSelector`, `CanvasViewer`, `ObjectSelector`), which render their respective parts of the 3-column layout.
3.  **Interaction**: `DungeonObjectInteraction` captures mouse events on the canvas and translates them into actions, such as selecting or moving an object.
4.  **Save**: Changes are propagated back through the `DungeonEditorSystem` to be written to the ROM.

## 3. Key Recent Fixes

The editor's current stability is the result of two major fixes:

### Critical Fix 1: The Coordinate System

-   **Problem**: Objects were rendering at twice their correct distance from the origin, often appearing outside the canvas entirely.
-   **Root Cause**: The code incorrectly assumed dungeon tiles were 16x16 pixels, using `* 16` for coordinate conversions. SNES dungeon tiles are **8x8 pixels**.
-   **The Fix**: All coordinate conversion functions in `dungeon_renderer.cc`, `dungeon_canvas_viewer.cc`, and `dungeon_object_interaction.cc` were corrected to use `* 8` and `/ 8`.

### Critical Fix 2: The Test Suite

-   **Problem**: The integration test suite was unusable, crashing with `SIGBUS` and `SIGSEGV` errors.
-   **Root Cause**: The `MockRom` implementation had severe memory management issues, causing crashes when test data was copied.
-   **The Fix**: The `MockRom` was **completely abandoned**. All 28 integration and unit tests were refactored to use a real `zelda3.sfc` ROM via the `TestRomManager`. This provides more realistic testing and resolved all crashes.

## 4. ROM Internals & Data Structures

This information is critical for understanding the editor's core logic and has been cross-referenced with the `usdasm` disassembly.

### Object Encoding

Dungeon objects are stored in one of three formats. The encoding logic is correctly implemented in `src/app/zelda3/dungeon/room_object.cc`.

-   **Type 1: Standard Objects (ID 0x00-0xFF)**
    -   **Format**: `xxxxxxss yyyyyyss iiiiiiii`
    -   **Use**: Common geometry like walls and floors.

-   **Type 2: Large Coordinate Objects (ID 0x100-0x1FF)**
    -   **Format**: `111111xx xxxxyyyy yyiiiiii`
    -   **Use**: More complex or interactive structures.

-   **Type 3: Special Objects (ID 0x200-0x27F)**
    -   **Format**: `xxxxxxii yyyyyyii 11111iii`
    -   **Use**: Critical gameplay elements like chests, switches, and bosses.

### Core Data Tables in ROM

-   **`bank_01.asm`**:
    -   **`DrawObjects` (0x018000)**: Master tables mapping an object's ID to its drawing routine.
    -   **`LoadAndBuildRoom` (0x01873A)**: The primary routine that reads and draws a room.
-   **`rooms.asm`**:
    -   **`RoomData_ObjectDataPointers` (0x1F8000)**: A table of 3-byte pointers to the object data for each of the 296 rooms.

## 5. Testing: 100% Pass Rate

The dungeon editor has comprehensive test coverage, ensuring its stability and correctness.

| Test Type         | Total | Passing | Pass Rate |
| ----------------- | ----- | ------- | --------- |
| **Unit Tests**    | 14    | 14      | 100% ✅    |
| **Integration**   | 14    | 14      | 100% ✅    |
| **E2E Tests**     | 1     | 1       | 100% ✅    |
| **TOTAL**         | **29**| **29**  | **100%** ✅ |

### How to Run Tests

1.  **Build the Tests**:
    ```bash
    cmake --preset macos-dev -B build_test
    cmake --build build_test --target yaze_test
    ```
2.  **Run All Dungeon Tests**:
    ```bash
    ./build_test/bin/yaze_test --gtest_filter="*Dungeon*"
    ```
3.  **Run E2E Smoke Test (Requires GUI)**:
    ```bash
    ./build_test/bin/yaze_test --show-gui --gtest_filter="*DungeonEditorSmokeTest*"
    ```

## 6. Dungeon Object Reference Tables

The following tables were generated by parsing the `DrawObjects` tables in `bank_01.asm`.

### Type 1 Object Reference Table

| ID (Hex) | ID (Dec) | Description (from assembly) |
| :--- | :--- | :--- |
| 0x00 | 0 | Rightwards 2x2 |
| 0x01 | 1 | Rightwards 2x4 |
| 0x02 | 2 | Rightwards 2x4 |
| 0x03 | 3 | Rightwards 2x4 spaced 4 |
| 0x04 | 4 | Rightwards 2x4 spaced 4 |
| 0x05 | 5 | Rightwards 2x4 spaced 4 (Both BG) |
| 0x06 | 6 | Rightwards 2x4 spaced 4 (Both BG) |
| 0x07 | 7 | Rightwards 2x2 |
| 0x08 | 8 | Rightwards 2x2 |
| 0x09 | 9 | Diagonal Acute |
| 0x0A | 10 | Diagonal Grave |
| 0x0B | 11 | Diagonal Grave |
| 0x0C | 12 | Diagonal Acute |
| 0x0D | 13 | Diagonal Acute |
| 0x0E | 14 | Diagonal Grave |
| 0x0F | 15 | Diagonal Grave |
| 0x10 | 16 | Diagonal Acute |
| 0x11 | 17 | Diagonal Acute |
| 0x12 | 18 | Diagonal Grave |
| 0x13 | 19 | Diagonal Grave |
| 0x14 | 20 | Diagonal Acute |
| 0x15 | 21 | Diagonal Acute (Both BG) |
| 0x16 | 22 | Diagonal Grave (Both BG) |
| 0x17 | 23 | Diagonal Grave (Both BG) |
| 0x18 | 24 | Diagonal Acute (Both BG) |
| 0x19 | 25 | Diagonal Acute (Both BG) |
| 0x1A | 26 | Diagonal Grave (Both BG) |
| 0x1B | 27 | Diagonal Grave (Both BG) |
| 0x1C | 28 | Diagonal Acute (Both BG) |
| 0x1D | 29 | Diagonal Acute (Both BG) |
| 0x1E | 30 | Diagonal Grave (Both BG) |
| 0x1F | 31 | Diagonal Grave (Both BG) |
| 0x20 | 32 | Diagonal Acute (Both BG) |
| 0x21 | 33 | Rightwards 1x2 |
| 0x22 | 34 | Rightwards Has Edge 1x1 |
| 0x23 | 35 | Rightwards Has Edge 1x1 |
| 0x24 | 36 | Rightwards Has Edge 1x1 |
| 0x25 | 37 | Rightwards Has Edge 1x1 |
| 0x26 | 38 | Rightwards Has Edge 1x1 |
| 0x27 | 39 | Rightwards Has Edge 1x1 |
| 0x28 | 40 | Rightwards Has Edge 1x1 |
| 0x29 | 41 | Rightwards Has Edge 1x1 |
| 0x2A | 42 | Rightwards Has Edge 1x1 |
| 0x2B | 43 | Rightwards Has Edge 1x1 |
| 0x2C | 44 | Rightwards Has Edge 1x1 |
| 0x2D | 45 | Rightwards Has Edge 1x1 |
| 0x2E | 46 | Rightwards Has Edge 1x1 |
| 0x2F | 47 | Rightwards Top Corners 1x2 |
| 0x30 | 48 | Rightwards Bottom Corners 1x2 |
| 0x31 | 49 | Nothing |
| 0x32 | 50 | Nothing |
| 0x33 | 51 | Rightwards 4x4 |
| 0x34 | 52 | Rightwards 1x1 Solid |
| 0x35 | 53 | Door Switcherer |
| 0x36 | 54 | Rightwards Decor 4x4 spaced 2 |
| 0x37 | 55 | Rightwards Decor 4x4 spaced 2 |
| 0x38 | 56 | Rightwards Statue 2x3 spaced 2 |
| 0x39 | 57 | Rightwards Pillar 2x4 spaced 4 |
| 0x3A | 58 | Rightwards Decor 4x3 spaced 4 |
| 0x3B | 59 | Rightwards Decor 4x3 spaced 4 |
| 0x3C | 60 | Rightwards Doubled 2x2 spaced 2 |
| 0x3D | 61 | Rightwards Pillar 2x4 spaced 4 |
| 0x3E | 62 | Rightwards Decor 2x2 spaced 12 |
| 0x3F | 63 | Rightwards Has Edge 1x1 |
| 0x40 | 64 | Rightwards Has Edge 1x1 |
| 0x41 | 65 | Rightwards Has Edge 1x1 |
| 0x42 | 66 | Rightwards Has Edge 1x1 |
| 0x43 | 67 | Rightwards Has Edge 1x1 |
| 0x44 | 68 | Rightwards Has Edge 1x1 |
| 0x45 | 69 | Rightwards Has Edge 1x1 |
| 0x46 | 70 | Rightwards Has Edge 1x1 |
| 0x47 | 71 | Waterfall |
| 0x48 | 72 | Waterfall |
| 0x49 | 73 | Rightwards Floor Tile 4x2 |
| 0x4A | 74 | Rightwards Floor Tile 4x2 |
| 0x4B | 75 | Rightwards Decor 2x2 spaced 12 |
| 0x4C | 76 | Rightwards Bar 4x3 |
| 0x4D | 77 | Rightwards Shelf 4x4 |
| 0x4E | 78 | Rightwards Shelf 4x4 |
| 0x4F | 79 | Rightwards Shelf 4x4 |
| 0x50 | 80 | Rightwards Line 1x1 |
| 0x51 | 81 | Rightwards Cannon Hole 4x3 |
| 0x52 | 82 | Rightwards Cannon Hole 4x3 |
| 0x53 | 83 | Rightwards 2x2 |
| 0x54 | 84 | Nothing |
| 0x55 | 85 | Rightwards Decor 4x2 spaced 8 |
| 0x56 | 86 | Rightwards Decor 4x2 spaced 8 |
| 0x57 | 87 | Nothing |
| 0x58 | 88 | Nothing |
| 0x59 | 89 | Nothing |
| 0x5A | 90 | Nothing |
| 0x5B | 91 | Rightwards Cannon Hole 4x3 |
| 0x5C | 92 | Rightwards Cannon Hole 4x3 |
| 0x5D | 93 | Rightwards Big Rail 1x3 |
| 0x5E | 94 | Rightwards Block 2x2 spaced 2 |
| 0x5F | 95 | Rightwards Has Edge 1x1 |
| 0x60 | 96 | Downwards 2x2 |
| 0x61 | 97 | Downwards 4x2 |
| 0x62 | 98 | Downwards 4x2 |
| 0x63 | 99 | Downwards 4x2 (Both BG) |
| 0x64 | 100 | Downwards 4x2 (Both BG) |
| 0x65 | 101 | Downwards Decor 4x2 spaced 4 |
| 0x66 | 102 | Downwards Decor 4x2 spaced 4 |
| 0x67 | 103 | Downwards 2x2 |
| 0x68 | 104 | Downwards 2x2 |
| 0x69 | 105 | Downwards Has Edge 1x1 |
| 0x6A | 106 | Downwards Edge 1x1 |
| 0x6B | 107 | Downwards Edge 1x1 |
| 0x6C | 108 | Downwards Left Corners 2x1 |
| 0x6D | 109 | Downwards Right Corners 2x1 |
| 0x6E | 110 | Nothing |
| 0x6F | 111 | Nothing |
| 0x70 | 112 | Downwards Floor 4x4 |
| 0x71 | 113 | Downwards 1x1 Solid |
| 0x72 | 114 | Nothing |
| 0x73 | 115 | Downwards Decor 4x4 spaced 2 |
| 0x74 | 116 | Downwards Decor 4x4 spaced 2 |
| 0x75 | 117 | Downwards Pillar 2x4 spaced 2 |
| 0x76 | 118 | Downwards Decor 3x4 spaced 4 |
| 0x77 | 119 | Downwards Decor 3x4 spaced 4 |
| 0x78 | 120 | Downwards Decor 2x2 spaced 12 |
| 0x79 | 121 | Downwards Edge 1x1 |
| 0x7A | 122 | Downwards Edge 1x1 |
| 0x7B | 123 | Downwards Decor 2x2 spaced 12 |
| 0x7C | 124 | Downwards Line 1x1 |
| 0x7D | 125 | Downwards 2x2 |
| 0x7E | 126 | Nothing |
| 0x7F | 127 | Downwards Decor 2x4 spaced 8 |
| 0x80 | 128 | Downwards Decor 2x4 spaced 8 |
| 0x81 | 129 | Downwards Decor 3x4 spaced 2 |
| 0x82 | 130 | Downwards Decor 3x4 spaced 2 |
| 0x83 | 131 | Downwards Decor 3x4 spaced 2 |
| 0x84 | 132 | Downwards Decor 3x4 spaced 2 |
| 0x85 | 133 | Downwards Cannon Hole 3x4 |
| 0x86 | 134 | Downwards Cannon Hole 3x4 |
| 0x87 | 135 | Downwards Pillar 2x4 spaced 2 |
| 0x88 | 136 | Downwards Big Rail 3x1 |
| 0x89 | 137 | Downwards Block 2x2 spaced 2 |
| 0x8A | 138 | Downwards Has Edge 1x1 |
| 0x8B | 139 | Downwards Edge 1x1 |
| 0x8C | 140 | Downwards Edge 1x1 |
| 0x8D | 141 | Downwards Edge 1x1 |
| 0x8E | 142 | Downwards Edge 1x1 |
| 0x8F | 143 | Downwards Bar 2x5 |
| 0x90 | 144 | Downwards 4x2 |
| 0x91 | 145 | Downwards 4x2 |
| 0x92 | 146 | Downwards 2x2 |
| 0x93 | 147 | Downwards 2x2 |
| 0x94 | 148 | Downwards Floor 4x4 |
| 0x95 | 149 | Downwards Pots 2x2 |
| 0x96 | 150 | Downwards Hammer Pegs 2x2 |
| 0x97 | 151 | Nothing |
| 0x98 | 152 | Nothing |
| 0x99 | 153 | Nothing |
| 0x9A | 154 | Nothing |
| 0x9B | 155 | Nothing |
| 0x9C | 156 | Nothing |
| 0x9D | 157 | Nothing |
| 0x9E | 158 | Nothing |
| 0x9F | 159 | Nothing |
| 0xA0 | 160 | Diagonal Ceiling Top Left A |
| 0xA1 | 161 | Diagonal Ceiling Bottom Left A |
| 0xA2 | 162 | Diagonal Ceiling Top Right A |
| 0xA3 | 163 | Diagonal Ceiling Bottom Right A |
| 0xA4 | 164 | Big Hole 4x4 |
| 0xA5 | 165 | Diagonal Ceiling Top Left B |
| 0xA6 | 166 | Diagonal Ceiling Bottom Left B |
| 0xA7 | 167 | Diagonal Ceiling Top Right B |
| 0xA8 | 168 | Diagonal Ceiling Bottom Right B |
| 0xA9 | 169 | Diagonal Ceiling Top Left B |
| 0xAA | 170 | Diagonal Ceiling Bottom Left B |
| 0xAB | 171 | Diagonal Ceiling Top Right B |
| 0xAC | 172 | Diagonal Ceiling Bottom Right B |
| 0xAD | 173 | Nothing |
| 0xAE | 174 | Nothing |
| 0xAF | 175 | Nothing |
| 0xB0 | 176 | Rightwards Edge 1x1 |
| 0xB1 | 177 | Rightwards Edge 1x1 |
| 0xB2 | 178 | Rightwards 4x4 |
| 0xB3 | 179 | Rightwards Has Edge 1x1 |
| 0xB4 | 180 | Rightwards Has Edge 1x1 |
| 0xB5 | 181 | Weird 2x4 |
| 0xB6 | 182 | Rightwards 2x4 |
| 0xB7 | 183 | Rightwards 2x4 |
| 0xB8 | 184 | Rightwards 2x2 |
| 0xB9 | 185 | Rightwards 2x2 |
| 0xBA | 186 | Rightwards 4x4 |
| 0xBB | 187 | Rightwards Block 2x2 spaced 2 |
| 0xBC | 188 | Rightwards Pots 2x2 |
| 0xBD | 189 | Rightwards Hammer Pegs 2x2 |
| 0xBE | 190 | Nothing |
| 0xBF | 191 | Nothing |
| 0xC0 | 192 | 4x4 Blocks In 4x4 Super Square |
| 0xC1 | 193 | Closed Chest Platform |
| 0xC2 | 194 | 4x4 Blocks In 4x4 Super Square |
| 0xC3 | 195 | 3x3 Floor In 4x4 Super Square |
| 0xC4 | 196 | 4x4 Floor One In 4x4 Super Square |
| 0xC5 | 197 | 4x4 Floor In 4x4 Super Square |
| 0xC6 | 198 | 4x4 Floor In 4x4 Super Square |
| 0xC7 | 199 | 4x4 Floor In 4x4 Super Square |
| 0xC8 | 200 | 4x4 Floor In 4x4 Super Square |
| 0xC9 | 201 | 4x4 Floor In 4x4 Super Square |
| 0xCA | 202 | 4x4 Floor In 4x4 Super Square |
| 0xCB | 203 | Nothing |
| 0xCC | 204 | Nothing |
| 0xCD | 205 | Moving Wall West |
| 0xCE | 206 | Moving Wall East |
| 0xCF | 207 | Nothing |
| 0xD0 | 208 | Nothing |
| 0xD1 | 209 | 4x4 Floor In 4x4 Super Square |
| 0xD2 | 210 | 4x4 Floor In 4x4 Super Square |
| 0xD3 | 211 | Check If Wall Is Moved |
| 0xD4 | 212 | Check If Wall Is Moved |
| 0xD5 | 213 | Check If Wall Is Moved |
| 0xD6 | 214 | Check If Wall Is Moved |
| 0xD7 | 215 | 3x3 Floor In 4x4 Super Square |
| 0xD8 | 216 | Water Overlay A 8x8 |
| 0xD9 | 217 | 4x4 Floor In 4x4 Super Square |
| 0xDA | 218 | Water Overlay B 8x8 |
| 0xDB | 219 | 4x4 Floor Two In 4x4 Super Square |
| 0xDC | 220 | Open Chest Platform |
| 0xDD | 221 | Table Rock 4x4 |
| 0xDE | 222 | Spike 2x2 In 4x4 Super Square |
| 0xDF | 223 | 4x4 Floor In 4x4 Super Square |
| 0xE0 | 224 | 4x4 Floor In 4x4 Super Square |
| 0xE1 | 225 | 4x4 Floor In 4x4 Super Square |
| 0xE2 | 226 | 4x4 Floor In 4x4 Super Square |
| 0xE3 | 227 | 4x4 Floor In 4x4 Super Square |
| 0xE4 | 228 | 4x4 Floor In 4x4 Super Square |
| 0xE5 | 229 | 4x4 Floor In 4x4 Super Square |
| 0xE6 | 230 | 4x4 Floor In 4x4 Super Square |
| 0xE7 | 231 | 4x4 Floor In 4x4 Super Square |
| 0xE8 | 232 | 4x4 Floor In 4x4 Super Square |
| 0xE9 | 233 | Nothing |
| 0xEA | 234 | Nothing |
| 0xEB | 235 | Nothing |
| 0xEC | 236 | Nothing |
| 0xED | 237 | Nothing |
| 0xEE | 238 | Nothing |
| 0xEF | 239 | Nothing |
| 0xF0 | 240 | Nothing |
| 0xF1 | 241 | Nothing |
| 0xF2 | 242 | Nothing |
| 0xF3 | 243 | Nothing |
| 0xF4 | 244 | Nothing |
| 0xF5 | 245 | Nothing |
| 0xF6 | 246 | Nothing |
| 0xF7 | 247 | Nothing |
| 0xF8 | 248 | Nothing |
| 0xF9 | 249 | Nothing |
| 0xFA | 250 | Nothing |
| 0xFB | 251 | Nothing |
| 0xFC | 252 | Nothing |
| 0xFD | 253 | Nothing |
| 0xFE | 254 | Nothing |
| 0xFF | 255 | Nothing |

### Type 2 Object Reference Table

| ID (Hex) | ID (Dec) | Description (from assembly) |
| :--- | :--- | :--- |
| 0x100 | 256 | 4x4 |
| 0x101 | 257 | 4x4 |
| 0x102 | 258 | 4x4 |
| 0x103 | 259 | 4x4 |
| 0x104 | 260 | 4x4 |
| 0x105 | 261 | 4x4 |
| 0x106 | 262 | 4x4 |
| 0x107 | 263 | 4x4 |
| 0x108 | 264 | 4x4 Corner (Both BG) |
| 0x109 | 265 | 4x4 Corner (Both BG) |
| 0x10A | 266 | 4x4 Corner (Both BG) |
| 0x10B | 267 | 4x4 Corner (Both BG) |
| 0x10C | 268 | 4x4 Corner (Both BG) |
| 0x10D | 269 | 4x4 Corner (Both BG) |
| 0x10E | 270 | 4x4 Corner (Both BG) |
| 0x10F | 271 | 4x4 Corner (Both BG) |
| 0x110 | 272 | Weird Corner Bottom (Both BG) |
| 0x111 | 273 | Weird Corner Bottom (Both BG) |
| 0x112 | 274 | Weird Corner Bottom (Both BG) |
| 0x113 | 275 | Weird Corner Bottom (Both BG) |
| 0x114 | 276 | Weird Corner Top (Both BG) |
| 0x115 | 277 | Weird Corner Top (Both BG) |
| 0x116 | 278 | Weird Corner Top (Both BG) |
| 0x117 | 279 | Weird Corner Top (Both BG) |
| 0x118 | 280 | Rightwards 2x2 |
| 0x119 | 281 | Rightwards 2x2 |
| 0x11A | 282 | Rightwards 2x2 |
| 0x11B | 283 | Rightwards 2x2 |
| 0x11C | 284 | 4x4 |
| 0x11D | 285 | Single 2x3 Pillar |
| 0x11E | 286 | Single 2x2 |
| 0x11F | 287 | Enabled Star Switch |
| 0x120 | 288 | Lit Torch |
| 0x121 | 289 | Single 2x3 Pillar |
| 0x122 | 290 | Bed 4x5 |
| 0x123 | 291 | Table Rock 4x3 |
| 0x124 | 292 | 4x4 |
| 0x125 | 293 | 4x4 |
| 0x126 | 294 | Single 2x3 Pillar |
| 0x127 | 295 | Rightwards 2x2 |
| 0x128 | 296 | Bed 4x5 |
| 0x129 | 297 | 4x4 |
| 0x12A | 298 | Portrait Of Mario |
| 0x12B | 299 | Rightwards 2x2 |
| 0x12C | 300 | Draw Rightwards 3x6 |
| 0x12D | 301 | Inter-Room Fat Stairs Up |
| 0x12E | 302 | Inter-Room Fat Stairs Down A |
| 0x12F | 303 | Inter-Room Fat Stairs Down B |
| 0x130 | 304 | Auto Stairs North Multi Layer A |
| 0x131 | 305 | Auto Stairs North Multi Layer B |
| 0x132 | 306 | Auto Stairs North Merged Layer A |
| 0x133 | 307 | Auto Stairs North Merged Layer B |
| 0x134 | 308 | Rightwards 2x2 |
| 0x135 | 309 | Water Hop Stairs A |
| 0x136 | 310 | Water Hop Stairs B |
| 0x137 | 311 | Dam Flood Gate |
| 0x138 | 312 | Spiral Stairs Going Up Upper |
| 0x139 | 313 | Spiral Stairs Going Down Upper |
| 0x13A | 314 | Spiral Stairs Going Up Lower |
| 0x13B | 315 | Spiral Stairs Going Down Lower |
| 0x13C | 316 | Sanctuary Wall |
| 0x13D | 317 | Table Rock 4x3 |
| 0x13E | 318 | Utility 6x3 |
| 0x13F | 319 | Magic Bat Altar |

### Type 3 Object Reference Table

| ID (Hex) | ID (Dec) | Description (from assembly) |
| :--- | :--- | :--- |
| 0x200 | 512 | Empty Water Face |
| 0x201 | 513 | Spitting Water Face |
| 0x202 | 514 | Drenching Water Face |
| 0x203 | 515 | Somaria Line (increment count) |
| 0x204 | 516 | Somaria Line |
| 0x205 | 517 | Somaria Line |
| 0x206 | 518 | Somaria Line |
| 0x207 | 519 | Somaria Line |
| 0x208 | 520 | Somaria Line |
| 0x209 | 521 | Somaria Line |
| 0x20A | 522 | Somaria Line |
| 0x20B | 523 | Somaria Line |
| 0x20C | 524 | Somaria Line |
| 0x20D | 525 | Prison Cell |
| 0x20E | 526 | Somaria Line (increment count) |
| 0x20F | 527 | Somaria Line |
| 0x210 | 528 | Rightwards 2x2 |
| 0x211 | 529 | Rightwards 2x2 |
| 0x212 | 530 | Rupee Floor |
| 0x213 | 531 | Rightwards 2x2 |
| 0x214 | 532 | Table Rock 4x3 |
| 0x215 | 533 | Kholdstare Shell |
| 0x216 | 534 | Hammer Peg Single |
| 0x217 | 535 | Prison Cell |
| 0x218 | 536 | Big Key Lock |
| 0x219 | 537 | Chest |
| 0x21A | 538 | Open Chest |
| 0x21B | 539 | Auto Stairs South Multi Layer A |
| 0x21C | 540 | Auto Stairs South Multi Layer B |
| 0x21D | 541 | Auto Stairs South Multi Layer C |
| 0x21E | 542 | Straight Inter-room Stairs Going Up North Upper |
| 0x21F | 543 | Straight Inter-room Stairs Going Down North Upper |
| 0x220 | 544 | Straight Inter-room Stairs Going Up South Upper |
| 0x221 | 545 | Straight Inter-room Stairs Going Down South Upper |
| 0x222 | 546 | Rightwards 2x2 |
| 0x223 | 547 | Rightwards 2x2 |
| 0x224 | 548 | Rightwards 2x2 |
| 0x225 | 549 | Rightwards 2x2 |
| 0x226 | 550 | Straight Inter-room Stairs Going Up North Lower |
| 0x227 | 551 | Straight Inter-room Stairs Going Down North Lower |
| 0x228 | 552 | Straight Inter-room Stairs Going Up South Lower |
| 0x229 | 553 | Straight Inter-room Stairs Going Down South Lower |
| 0x22A | 554 | Lamp Cones |
| 0x22B | 555 | Weird Glove Required Pot |
| 0x22C | 556 | Big Gray Rock |
| 0x22D | 557 | Agahnims Altar |
| 0x22E | 558 | Agahnims Windows |
| 0x22F | 559 | Single Pot |
| 0x230 | 560 | Weird Ugly Pot |
| 0x231 | 561 | Big Chest |
| 0x232 | 562 | Open Big Chest |
| 0x233 | 563 | Auto Stairs South Merged Layer |
| 0x234 | 564 | Chest Platform Vertical Wall |
| 0x235 | 565 | Chest Platform Vertical Wall |
| 0x236 | 566 | Draw Rightwards 3x6 |
| 0x237 | 567 | Draw Rightwards 3x6 |
| 0x238 | 568 | Chest Platform Vertical Wall |
| 0x239 | 569 | Chest Platform Vertical Wall |
| 0x23A | 570 | Vertical Turtle Rock Pipe |
| 0x23B | 571 | Vertical Turtle Rock Pipe |
| 0x23C | 572 | Horizontal Turtle Rock Pipe |
| 0x23D | 573 | Horizontal Turtle Rock Pipe |
| 0x23E | 574 | Rightwards 2x2 |
| 0x23F | 575 | Rightwards 2x2 |
| 0x240 | 576 | Rightwards 2x2 |
| 0x241 | 577 | Rightwards 2x2 |
| 0x242 | 578 | Rightwards 2x2 |
| 0x243 | 579 | Rightwards 2x2 |
| 0x244 | 580 | Rightwards 2x2 |
| 0x245 | 581 | Rightwards 2x2 |
| 0x246 | 582 | Rightwards 2x2 |
| 0x247 | 583 | Bombable Floor |
| 0x248 | 584 | 4x4 |
| 0x249 | 585 | Rightwards 2x2 |
| 0x24A | 586 | Rightwards 2x2 |
| 0x24B | 587 | Big Wall Decor |
| 0x24C | 588 | Smithy Furnace |
| 0x24D | 589 | Utility 6x3 |
| 0x24E | 590 | Table Rock 4x3 |
| 0x24F | 591 | Rightwards 2x2 |
| 0x250 | 592 | Single 2x2 |
| 0x251 | 593 | Rightwards 2x2 |
| 0x252 | 594 | Rightwards 2x2 |
| 0x253 | 595 | Rightwards 2x2 |
| 0x254 | 596 | Fortune Teller Room |
| 0x255 | 597 | Utility 3x5 |
| 0x256 | 598 | Rightwards 2x2 |
| 0x257 | 599 | Rightwards 2x2 |
| 0x258 | 600 | Rightwards 2x2 |
| 0x259 | 601 | Rightwards 2x2 |
| 0x25A | 602 | Table Bowl |
| 0x25B | 603 | Utility 3x5 |
| 0x25C | 604 | Horizontal Turtle Rock Pipe |
| 0x25D | 605 | Utility 6x3 |
| 0x25E | 606 | Rightwards 2x2 |
| 0x25F | 607 | Rightwards 2x2 |
| 0x260 | 608 | Archery Game Target Door |
| 0x261 | 609 | Archery Game Target Door |
| 0x262 | 610 | Vitreous Goo Graphics |
| 0x263 | 611 | Rightwards 2x2 |
| 0x264 | 612 | Rightwards 2x2 |
| 0x265 | 613 | Rightwards 2x2 |
| 0x266 | 614 | 4x4 |
| 0x267 | 615 | Table Rock 4x3 |
| 0x268 | 616 | Table Rock 4x3 |
| 0x269 | 617 | Solid Wall Decor 3x4 |
| 0x26A | 618 | Solid Wall Decor 3x4 |
| 0x26B | 619 | 4x4 |
| 0x26C | 620 | Table Rock 4x3 |
| 0x26D | 621 | Table Rock 4x3 |
| 0x26E | 622 | Solid Wall Decor 3x4 |
| 0x26F | 623 | Solid Wall Decor 3x4 |
| 0x270 | 624 | Light Beam On Floor |
| 0x271 | 625 | Big Light Beam On Floor |
| 0x272 | 626 | Trinexx Shell |
| 0x273 | 627 | BG2 Mask Full |
| 0x274 | 628 | Floor Light |
| 0x275 | 629 | Rightwards 2x2 |
| 0x276 | 630 | Big Wall Decor |
| 0x277 | 631 | Big Wall Decor |
| 0x278 | 632 | Ganon Triforce Floor Decor |
| 0x279 | 633 | Table Rock 4x3 |
| 0x27A | 634 | 4x4 |
| 0x27B | 635 | Vitreous Goo Damage |
| 0x27C | 636 | Rightwards 2x2 |
| 0x27D | 637 | Rightwards 2x2 |
| 0x27E | 638 | Rightwards 2x2 |
| 0x27F | 639 | Nothing |
