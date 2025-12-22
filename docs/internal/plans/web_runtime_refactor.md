# YAZE Web Runtime Refactoring & Optimization Plan

**Date:** November 25, 2025
**Target Component:** `src/web` (WASM Runtime & UI Layer)
**Status:** Draft

## 1. Executive Summary

The YAZE web runtime is a sophisticated application bridging C++ (via Emscripten) with modern Web APIs. It features advanced capabilities like PWA support, touch gestures, and a DOM-mirroring system for AI agents.

However, the current codebase suffers from **initialization fragility** (reliance on `setInterval` polling) and **architectural coupling** (massive inline scripts in `shell.html`, duplicate logic modules). This plan outlines the steps to stabilize the startup sequence, modularize the UI logic, and consolidate redundant features.

---

## 2. Current State Analysis

### 2.1. Strengths
*   **Namespace Architecture:** `src/web/core/namespace.js` provides a solid foundation for organizing globals.
*   **Agent Readiness:** The `widget_overlay.js` and `agent_automation.js` components are forward-thinking, enabling DOM-based agents to "see" the canvas.
*   **Performance:** Excellent implementation of Service Workers (`stale-while-revalidate`) and AudioWorklets.

### 2.2. Critical Issues
1.  **Initialization Race Conditions:** Components like `FilesystemManager` and `terminal.js` poll for `Module` readiness using `setInterval`. This is non-deterministic and wastes cycles.
2.  **`shell.html` Bloat:** The main HTML file contains ~500 lines of inline JavaScript handling UI settings, menus, and AI tools. This creates circular dependencies (Terminal depends on Shell) and violates CSP best practices.
3.  **Logic Duplication:**
    *   **Collaboration:** `collab_console.js` (JS WebSocket) vs `collaboration_ui.js` (C++ Bindings).
    *   **Drag & Drop:** `drop_zone.js` (JS Implementation) vs `WasmDropHandler` (C++ Implementation).
4.  **Global Namespace Pollution:** Despite having `window.yaze`, many components still attach directly to `window` or rely on Emscripten's global `Module`.

---

## 3. Improvement Plan

### Phase 1: Stabilization (Initialization Architecture)
**Goal:** Replace polling with a deterministic Event/Promise chain.

1.  **Centralize Boot Sequence:**
    *   Modify `src/web/core/namespace.js` to expose a `yaze.core.boot()` Promise.
    *   Refactor `app.js` to resolve this Promise only when `Module.onRuntimeInitialized` fires.
2.  **Refactor Dependent Components:**
    *   Update `FilesystemManager` to await `yaze.core.boot()` instead of polling.
    *   Update `terminal.js` to listen for the `yaze:ready` event instead of checking `isModuleReady` via interval.

### Phase 2: Decoupling (Shell Extraction)
**Goal:** Remove inline JavaScript from `shell.html`.

1.  **Extract UI Controller:**
    *   Create `src/web/core/ui_controller.js`.
    *   Move Settings modal logic, Theme switching, and Layout toggling from `shell.html` to this new file.
2.  **Relocate AI Tools:**
    *   Move the `aiTools` object definitions from `shell.html` to `src/web/core/agent_automation.js`.
    *   Ensure `terminal.js` references `window.yaze.ai` instead of the global `aiTools`.
3.  **Clean `shell.html`:**
    *   Replace inline `onclick` handlers with event listeners attached in `ui_controller.js`.

### Phase 3: Consolidation (Redundancy Removal)
**Goal:** Establish "Single Sources of Truth".

1.  **Collaboration Unification:**
    *   Designate `components/collaboration_ui.js` (C++ Bindings) as the primary implementation.
    *   Deprecate `collab_console.js` or repurpose it strictly as a UI view for the C++ backend, removing its direct WebSocket networking code.
2.  **Drop Zone Cleanup:**
    *   Modify `drop_zone.js` to act purely as a visual overlay.
    *   Pass drop events directly to the C++ `WasmDropHandler` via `Module.ccall`, removing the JS-side file parsing logic unless it serves as a specific fallback.

---

## 4. Technical Implementation Steps

### Step 4.1: Create UI Controller
**File:** `src/web/core/ui_controller.js`

```javascript
(function() {
    'use strict';
    
    window.yaze.ui.controller = {
        init: function() {
            this.bindEvents();
            this.loadSettings();
        },
        
        bindEvents: function() {
            // Move event listeners here
            document.getElementById('settings-btn').addEventListener('click', this.showSettings);
            // ...
        },
        
        // Move settings logic here
        showSettings: function() { ... }
    };
    
    // Auto-init on DOM ready
    document.addEventListener('DOMContentLoaded', () => window.yaze.ui.controller.init());
})();
```

### Step 4.2: Refactor Initialization (Namespace)
**File:** `src/web/core/namespace.js`

Add a boot promise mechanism:

```javascript
window.yaze.core.bootPromise = new Promise((resolve) => {
    window.yaze._resolveBoot = resolve;
});

window.yaze.core.ready = function() {
    return window.yaze.core.bootPromise;
};
```

**File:** `src/web/app.js`

Trigger the boot:

```javascript
Module.onRuntimeInitialized = function() {
    // ... existing initialization ...
    window.yaze._resolveBoot(Module);
    window.yaze.events.emit('ready', Module);
};
```

### Step 4.3: Clean Shell HTML
Remove the `<script>` block at the bottom of `src/web/shell.html` and replace it with:
```html
<script src="core/ui_controller.js"></script>
```

---

## 5. Verification Strategy

1.  **Startup Test:**
    *   Load the page with Network throttling (Slow 3G).
    *   Verify no errors appear in the console regarding "Module not defined" or "FS not ready".
    *   Confirm `FilesystemManager` initializes without retries.

2.  **Feature Test:**
    *   Open "Settings" modal (verifies `ui_controller.js` migration).
    *   Type `/ai app-state` in the terminal (verifies `aiTools` migration).
    *   Drag and drop a ROM file (verifies Drop Zone integration).

3.  **Agent Test:**
    *   Execute `window.yaze.gui.discover()` in the console.
    *   Verify it returns the JSON tree of ImGui widgets.

---

## 6. Action Checklist

- [ ] **Create** `src/web/core/ui_controller.js`.
- [ ] **Refactor** `src/web/core/namespace.js` to include boot Promise.
- [ ] **Modify** `src/web/app.js` to resolve boot Promise on init.
- [ ] **Move** `aiTools` from `shell.html` to `src/web/core/agent_automation.js`.
- [ ] **Move** Settings/UI logic from `shell.html` to `src/web/core/ui_controller.js`.
- [ ] **Clean** `src/web/shell.html` (remove inline scripts).
- [ ] **Refactor** `src/web/core/filesystem_manager.js` to await boot Promise.
- [ ] **Update** `src/web/pwa/service-worker.js` to cache new `ui_controller.js`.
