# Release Notes

## v0.5.0-alpha (In Development)

**Type:** Major Feature & Architecture Update
**Date:** TBD

### 🎵 New Music Editor
The highly anticipated SPC Music Editor is now available!
- **Tracker View:** Edit music patterns with a familiar tracker interface.
- **Piano Roll:** Visualize and edit notes with velocity and duration control.
- **Preview:** Real-time N-SPC audio preview with authentic ADSR envelopes.
- **Instruments:** Manage instruments and samples directly.

### 🕸️ Web App Improvements (WASM)
- **Experimental Web Port:** Run YAZE directly in your browser.
- **UI Refresh:** New panelized layout with VSCode-style terminal and problems view.
- **Usability:** Improved error handling for missing features (Emulator).
- **Security:** Hardened `SharedArrayBuffer` support with `coi-serviceworker`.

### 🏗️ Architecture & CI
- **Windows CI:** Fixed environment setup for `clang-cl` builds (MSVC dev cmd).
- **Code Quality:** Consolidated CI workflows.
- **SDL3 Prep:** Groundwork for the upcoming migration to SDL3.

---

## v0.4.0 - Music Editor & UI Polish
**Released:** November 2025

- Complete SPC music editing infrastructure.
- EditorManager refactoring for better multi-window support.
- AI Agent integration (experimental).
