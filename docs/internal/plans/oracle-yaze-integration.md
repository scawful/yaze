# Oracle of Secrets & YAZE Integration Plan

## Overview
This document outlines the roadmap for enhancing `yaze` to better support the `Oracle of Secrets` ROM hack development workflow, specifically focusing on Assembly editing, version control, and AI integration.

## 1. Enhanced Assembly Editor

### A. Symbol Navigation & Autocomplete
*   **Goal:** Enable "Go to Definition" and autocomplete for labels and constants.
*   **Implementation:**
    *   Parse `asar` symbol files (e.g., `*.sym` or `*.symbols` generated via `--symbols=wla`).
    *   Index labels, defines, and macros from the source code (`.asm` files).
    *   Integrate with the editor's text view to provide clickable links or hover information.

### B. Error Highlighting
*   **Goal:** Map `asar` build errors directly to lines in the code editor.
*   **Implementation:**
    *   Capture `stdout` and `stderr` from the `asar` build process.
    *   Parse standard `asar` error formats (e.g., `error: (file.asm:123) ...`).
    *   Visualize errors using squiggly lines or markers in the gutter.
    *   Populate a "Problems" or "Build Output" panel with clickable error entries.

### C. Macro & Snippet Library
*   **Goal:** Speed up coding with common OOS/ASM patterns.
*   **Implementation:**
    *   Add a snippet system to `AssemblyEditor`.
    *   Include built-in snippets for:
        *   `pushpc` / `pullpc` hooks.
        *   `%Set_Sprite_Properties` macros.
        *   Standard routine headers (preserve registers, `PHP`/`PLP`).
    *   Allow user-defined snippets in `Oracle-of-Secrets.yaze` project settings.

## 2. Lightweight Version Management

### A. Snapshot Feature
*   **Goal:** Quick local commits without leaving the editor.
*   **Implementation:**
    *   Add a "Snapshot" button to the main toolbar.
    *   Command: `git add . && git commit -m "Snapshot: <Timestamp>"`
    *   Display a success/failure notification.

### B. Diff View
*   **Goal:** See what changed since the last commit.
*   **Implementation:**
    *   Add "Compare with HEAD" context menu option for files.
    *   Render a side-by-side or inline diff using `git diff`.

## 3. AI-Driven Idea Management

### A. Context Injection
*   **Goal:** Keep the AI aligned with the current task.
*   **Implementation:**
    *   Utilize `.gemini/yaze_agent_prompt.md` (already created).
    *   Allow the agent to read `oracle.org` (ToDo list) to understand current priorities.

### B. Memory Map Awareness
*   **Goal:** Prevent memory conflicts.
*   **Implementation:**
    *   Grant the agent specific tool access to query `Docs/Core/MemoryMap.md`.
    *   Implement a "Check Free RAM" tool that parses the memory map.

## 4. Proposed Script Changes (Oracle of Secrets)

To support the features above, the `run.sh` script in `Oracle-of-Secrets` needs to be updated to generate symbols.

```bash
#!/bin/bash
ROM_VERSION=168
ROM_PATH="Roms/oos${ROM_VERSION}.sfc"
# Generate WLA symbols for yaze integration
asar --symbols=wla Oracle_main.asm "$ROM_PATH"
# Rename to .symbols if yaze expects that, or configure yaze to read .sym
mv "Roms/oos${ROM_VERSION}.sym" "Roms/oos${ROM_VERSION}.symbols"
```

## 5. Implemented Debugging Workflows

### A. Oracle RAM Live Panel
*   **Status:** ✅ Completed
*   **Features:**
    *   30Hz live sync with Mesen2 memory.
    *   Symbolic label resolution for OoS-specific addresses ($7E0010, $7E0739, etc.).
    *   Integrated with `AgentUiController` and `PanelManager`.

### B. ASM Follow Mode
*   **Status:** ✅ Completed (Prototype)
*   **Features:**
    *   Real-time PC synchronization between emulator and source.
    *   Symbol-to-source mapping via `AsmFollowService`.
    *   `SymbolProvider` extensions for file/line lookup.

## Next Steps
1.  **Dungeon Context Awareness:** Implement automatic watch updates based on room transitions.
2.  **Sprite AI Regression Tests:** Utilize Mesen's `BATCH` command for automated validation.
