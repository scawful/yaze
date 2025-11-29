# Web DOM Interaction & Color Picker Report

## Overview
This document details the investigation into the Web DOM structure of the YAZE WASM application, the interaction with the ImGui-based Color Picker, and the recommended workflow for future agents.

## Web DOM Structure
The YAZE WASM application is primarily an ImGui application rendered onto an HTML5 Canvas.
- **Canvas Element**: The main interaction point is the `<canvas>` element (ID `canvas`).
- **DOM Elements**: There are very few standard DOM elements for UI controls. Most UI is rendered by ImGui within the canvas.
- **Input**: Interaction relies on mouse events (clicks, drags) and keyboard input sent to the canvas.

## Color Picker Investigation
- **Initial Request**: The user mentioned a "web color picker".
- **Findings**:
    - No standard HTML `<input type="color">` or JavaScript-based color picker was found in `src/web`.
    - The color picker is part of the ImGui interface (`PaletteEditorWidget`).
    - It appears as a popup window ("Edit Color") when a color swatch is clicked.
- **Fix Implemented**:
    - Standardized `PaletteEditorWidget` to use `gui::SnesColorEdit4` instead of manual `ImGui::ColorEdit3`.
    - Used `gui::MakePopupIdWithInstance` to generate unique IDs for the "Edit Color" popup, preventing conflicts when multiple editors are open.
    - Verified the fix by rebuilding the WASM app and interacting with it via the browser subagent.

## Recommended Editing Flow for Agents
Since the application is heavily ImGui-based, standard DOM manipulation tools (`click_element`, `fill_input`) are of limited use for the core application features.

### 1. Navigation & Setup
- **Navigate**: Use `open_browser_url` to go to `http://localhost:8080`.
- **Wait**: Always wait for the WASM module to load (look for "Ready" in console or wait 5-10 seconds).
- **ROM Loading**:
    - Drag and drop is the most reliable way to load a ROM if `window.yaze.control.loadRom` is not available or robust.
    - Use `browser_drag_file_to_pixel` (or similar) to drop `zelda3.sfc` onto the canvas center.

### 2. Interacting with ImGui
- **JavaScript Bridge**: Use `window.yaze.control.*` APIs to switch editors and query state.
    - Example: `window.yaze.control.switchEditor('Palette')`
- **Pixel-Based Interaction**:
    - Use `click_browser_pixel` and `browser_drag_pixel_to_pixel` to interact with ImGui elements.
    - **Coordinates**: You may need to infer coordinates or use a "visual search" approach (taking screenshots and analyzing them) to find buttons.
    - **Feedback**: Take screenshots after actions to verify the UI updated as expected.

### 3. Debugging & Inspection
- **Console Logs**: Use `capture_browser_console_logs` to check for WASM errors or status messages.
- **Screenshots**: Essential for verifying rendering and UI state.

## Key Files
- `src/app/gui/widgets/palette_editor_widget.cc`: Implements the Palette Editor UI.
- `src/web/app.js`: Main JavaScript entry point, exposes `window.yaze` API.
- `docs/internal/wasm-yazeDebug-api-reference.md`: Reference for the JavaScript API.
