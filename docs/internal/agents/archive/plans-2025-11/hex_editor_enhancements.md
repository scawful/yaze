# Plan: Hex Editor Enhancements (Inspired by ImHex)

**Status:** Active  
**Owner (Agent ID):** imgui-frontend-engineer  
**Last Updated:** 2025-11-25  
**Next Review:** 2025-12-02  
**Coordination Board Entry:** link when claimed

This document outlines the roadmap for enhancing the `yaze` Memory/Hex Editor to provide robust analysis tools similar to ImHex.

## Phase 1: Data Inspector (High Priority)

**Goal:** Provide immediate context for the selected byte(s) in the Hex Editor without mental math.

**Implementation Steps:**
1.  **Create `DataInspector` Component:**
    *   A standalone ImGui widget (`src/app/editor/code/data_inspector.h/cc`).
    *   Accepts a `const uint8_t* data_ptr` and `size_t max_len`.
2.  **Standard Types:**
    *   Decode and display Little Endian values:
        *   `int8_t` / `uint8_t`
        *   `int16_t` / `uint16_t`
        *   `int24_t` / `uint24_t` (Common SNES pointers)
        *   `int32_t` / `uint32_t`
    *   Display Binary representation (`00001111`).
3.  **SNES-Specific Types (The "Yaze" Value):**
    *   **SNES LoROM Address:** Convert the physical offset to `$BB:AAAA` format.
    *   **RGB555 Color:** Interpret 2 bytes as `0bbbbbgggggrrrrr`. Show a colored rectangle preview.
    *   **Tile Attribute:** Interpret byte as `vhopppcc` (Vertical/Horizontal flip, Priority, Palette, Tile High bit).
4.  **Integration:**
    *   Modify `MemoryEditorWithDiffChecker` to instantiate and render `DataInspector` in a sidebar or child window next to the main hex grid.
    *   Hook into the hex editor's "Selection Changed" event (or poll selection state) to update the Inspector.

## Phase 2: Entropy Navigation (Navigation)

**Goal:** Visualize the ROM's structure to quickly find free space, graphics, or code.

**Implementation Steps:**
1.  **Entropy Calculator:**
    *   Create a utility to calculate Shannon entropy for blocks of data (e.g., 256-byte chunks).
    *   Run this calculation in a background thread when a ROM is loaded to generate an `EntropyMap`.
2.  **Minimap Widget:**
    *   Render a thin vertical bar next to the hex scrollbar.
    *   Map the file offset to vertical pixels.
    *   **Color Coding:**
        *   **Black:** Zeroes (`0x00` fill).
        *   **Dark Grey:** `0xFF` fill (common flash erase value).
        *   **Blue:** Low entropy (Text, Tables).
        *   **Red:** High entropy (Compressed Graphics, Code).
3.  **Interaction:**
    *   Clicking the minimap jumps the Hex Editor to that offset.

## Phase 3: Structure Templates (Advanced Analysis)

**Goal:** Define and visualize complex data structures on top of the raw hex.

**Implementation Steps:**
1.  **Template Definition System:**
    *   Define a C++-based schema builder (e.g., `StructBuilder("Header").AddString("Title", 21).AddByte("MapMode")...`).
2.  **Visualizer:**
    *   Render these structures as a tree view (ImGui TreeNodes).
    *   When a tree node is hovered, highlight the corresponding bytes in the Hex Editor grid.
3.  **Standard Templates:**
    *   Implement templates for known ALTTP structures: `SNES Header`, `Dungeon Header`, `Sprite Properties`.

## Phase 4: Disassembly Integration

**Goal:** Seamless transition between data viewing and code analysis.

**Implementation Steps:**
1.  **Context Menu:** Add "Disassemble Here" to the Hex Editor right-click menu.
2.  **Disassembly View:**
    *   Invoke the `disassembler` (already present in `app/emu/debug`) on the selected range.
    *   Display the output in a popup or switch to the Assembly Editor.

---

## Initial Work Item: Data Inspector

We will begin with Phase 1. This requires creating the `DataInspector` class and hooking it into `MemoryEditorWithDiffChecker`.
