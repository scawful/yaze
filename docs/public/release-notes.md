# Release Notes

## v0.5.0 (Released December 2025)

**Type:** Platform Expansion & Stability
**Date:** 2025-12-28

### 🧩 Graphics & Palette Accuracy
- Fixed palette conversion and Tile16 tint regressions.
- Corrected palette slicing for graphics sheets and indexed → SNES planar conversion.
- Stabilized overworld palette/tilemap saves and render service GameData loads.

### 🧭 Editor UX & Reliability
- Refined dashboard/editor selection layouts and card rendering.
- Moved the layout designer into a lab target for safer experimentation.
- Hardened room loading APIs and added room count reporting for C API consumers.

### 🤖 Automation & AI
- Refactored gRPC agent services and editor wiring.
- Hardened CLI patch handling and tool output formatting.

### 📦 Platform & Build
- Added iOS platform scaffolding (experimental).
- Added build helper scripts and simplified nightly workflow.
- Refreshed toolchain/dependency wiring and standardized build directory policy.

### 🧪 Testing
- Added role-based ROM selection and ROM-availability gating.
- Stabilized rendering/benchmark tests and aligned integration expectations.

---

## v0.4.0 - Music Editor & UI Polish
**Released:** November 2025

- Complete SPC music editing infrastructure.
- EditorManager refactoring for better multi-window support.
- AI Agent integration (experimental).
