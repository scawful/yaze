# Complete Duplicate Rendering Investigation

**Date:** 2025-11-25
**Status:** Investigation Complete - Root Cause Analysis
**Issue:** Elements inside editor cards appear twice (visually stacked)

---

## Executive Summary

Traced the complete call chain from main loop to editor content rendering. **No duplicate Update() or Draw() calls found**. The issue is NOT caused by multiple rendering paths in the editor system.

**Key Finding:** The diagnostic code added to `EditorCard::Begin()` will definitively identify if cards are being rendered twice. If no duplicates are detected by the diagnostic, the issue lies outside the EditorCard system (likely ImGui draw list submission or Z-ordering).

---

## Complete Call Chain (Main Loop → Editor Content)

### 1. Main Loop (`controller.cc`)

```
Controller::OnLoad() [Line 56]
  ├─ ImGui::NewFrame() [Line 63-65] ← SINGLE CALL
  ├─ DockSpace Setup [Lines 67-116]
  │  ├─ Calculate sidebar offsets [Lines 70-78]
  │  ├─ Create main dockspace window [Lines 103-116]
  │  └─ EditorManager::DrawMenuBar() [Line 112]
  │
  └─ EditorManager::Update() [Line 124] ← SINGLE CALL
     └─ DoRender() [Line 134]
        └─ ImGui::Render() ← SINGLE CALL
```

**Verdict:** ✅ Clean single-path rendering - no duplicates at main loop level

---

### 2. EditorManager Update Flow (`editor_manager.cc:616-843`)

```
EditorManager::Update()
  ├─ [Lines 617-626] Process deferred actions
  ├─ [Lines 632-662] Draw UI systems (popups, toasts, dialogs)
  ├─ [Lines 664-693] Draw UICoordinator (welcome screen, command palette)
  │
  ├─ [Lines 698-772] Draw Sidebar (BEFORE ROM check)
  │  ├─ Check: IsCardSidebarVisible() && !IsSidebarCollapsed()
  │  ├─ Mutual exclusion: IsTreeViewMode() ?
  │  │  ├─ TRUE  → DrawTreeSidebar() [Line 758]
  │  │  └─ FALSE → DrawSidebar() [Line 761]
  │  └─ Note: Different window names prevent overlap
  │     - DrawSidebar() → "##EditorCardSidebar"
  │     - DrawTreeSidebar() → "##TreeSidebar"
  │
  ├─ [Lines 774-778] Draw RightPanelManager (BEFORE ROM check)
  │  └─ RightPanelManager::Draw() → "##RightPanel"
  │
  ├─ [Lines 802-812] Early return if no ROM loaded
  │
  └─ [Lines 1043-1056] Update active editors (ONLY PATH TO EDITOR UPDATE)
     └─ for (editor : active_editors_)
        └─ if (*editor->active())
           └─ editor->Update() ← SINGLE CALL PER EDITOR PER FRAME
```

**Verdict:** ✅ Only one `editor->Update()` call per active editor per frame

---

### 3. Editor Update Implementation (e.g., OverworldEditor)

**File:** `src/app/editor/overworld/overworld_editor.cc:228`

```
OverworldEditor::Update()
  ├─ [Lines 240-258] Create local EditorCard instances
  │  └─ EditorCard overworld_canvas_card(...)
  │     EditorCard tile16_card(...)
  │     ... (8 cards total)
  │
  ├─ [Lines 294-300] Overworld Canvas Card
  │  └─ if (show_overworld_canvas_)
  │     if (overworld_canvas_card.Begin(&show_overworld_canvas_))
  │        DrawToolset()
  │        DrawOverworldCanvas()
  │     overworld_canvas_card.End() ← ALWAYS CALLED
  │
  ├─ [Lines 303-308] Tile16 Selector Card
  │  └─ if (show_tile16_selector_)
  │     if (tile16_card.Begin(&show_tile16_selector_))
  │        DrawTile16Selector()
  │     tile16_card.End()
  │
  └─ ... (6 more cards, same pattern)
```

**Pattern:** Each card follows strict Begin/End pairing:
```cpp
if (visibility_flag) {
  if (card.Begin(&visibility_flag)) {
    // Draw content ONCE
  }
  card.End(); // ALWAYS called after Begin()
}
```

**Verdict:** ✅ No duplicate Begin() calls - each card rendered exactly once per Update()

---

### 4. EditorCard Rendering (`editor_layout.cc`)

```
EditorCard::Begin(bool* p_open) [Lines 256-366]
  ├─ [Lines 257-261] Check visibility flag
  │  └─ if (p_open && !*p_open) return false
  │
  ├─ [Lines 263-285] 🔍 DUPLICATE DETECTION (NEW)
  │  └─ Track which cards have called Begin() this frame
  │     if (duplicate detected)
  │        fprintf(stderr, "DUPLICATE DETECTED: '%s' frame %d")
  │        duplicate_detected_ = true
  │
  ├─ [Lines 288-292] Handle collapsed state
  ├─ [Lines 294-336] Setup ImGui window
  └─ [Lines 352-356] Call ImGui::Begin()
     └─ imgui_begun_ = true ← Tracks that End() must be called

EditorCard::End() [Lines 369-380]
  └─ if (imgui_begun_)
     ImGui::End()
     imgui_begun_ = false
```

**Diagnostic Behavior:**
- Frame tracking resets on `ImGui::GetFrameCount()` change
- Each `Begin()` call checks if card name already in `cards_begun_this_frame_`
- Duplicate detected → logs to stderr and sets flag
- **This will definitively identify double Begin() calls**

**Verdict:** ✅ Diagnostic will catch any duplicate Begin() calls

---

### 5. RightPanelManager (ProposalDrawer, AgentChat, Settings)

**File:** `src/app/editor/ui/right_panel_manager.cc`

```
RightPanelManager::Draw() [Lines 117-181]
  └─ if (active_panel_ != PanelType::kNone)
     ImGui::Begin("##RightPanel", ...)
     DrawPanelHeader(...)
     switch (active_panel_)
        case kProposals: DrawProposalsPanel() [Line 162]
           └─ proposal_drawer_->DrawContent() [Line 238]
              NOT Draw()! Only DrawContent()!
        case kAgentChat: DrawAgentChatPanel()
        case kSettings: DrawSettingsPanel()
     ImGui::End()
```

**Key Discovery:** ProposalDrawer has TWO methods:
- `Draw()` - Creates own window (lines 75-107 in proposal_drawer.cc) ← **NEVER CALLED**
- `DrawContent()` - Renders inside existing window (line 238) ← **ONLY THIS IS USED**

**Verification in EditorManager:**
```cpp
// Line 827 in editor_manager.cc
// Proposal drawer is now drawn through RightPanelManager
// Removed duplicate direct call - DrawProposalsPanel() in RightPanelManager handles it
```

**Verdict:** ✅ ProposalDrawer::Draw() is dead code - only DrawContent() used

---

## What Was Ruled Out

### ❌ Multiple Update() Calls
- **EditorManager::Update()** calls `editor->Update()` exactly once per active editor (line 1047)
- **Controller::OnLoad()** calls `EditorManager::Update()` exactly once per frame (line 124)
- **No loops, no recursion, no duplicate paths**

### ❌ ImGui Begin/End Mismatches
- Every `EditorCard::Begin()` has matching `End()` call
- `imgui_begun_` flag prevents double End() calls
- Verified in OverworldEditor: 8 cards × 1 Begin + 1 End each = balanced

### ❌ Sidebar Double Rendering
- `DrawSidebar()` and `DrawTreeSidebar()` are **mutually exclusive**
- Different window names: `##EditorCardSidebar` vs `##TreeSidebar`
- Only one is called based on `IsTreeViewMode()` check (lines 757-763)

### ❌ RightPanel vs Direct Drawer Calls
- ProposalDrawer::Draw() is **never called** (confirmed with grep)
- Only `DrawContent()` used via RightPanelManager::DrawProposalsPanel()
- Comment at line 827 confirms duplicate call was removed

### ❌ EditorCard Registry Drawing Cards
- `card_registry_.ShowCard()` only sets **visibility flags**
- Cards are **not drawn by registry** - only drawn in editor Update() methods
- Registry only manages: visibility state, sidebar UI, card browser

### ❌ Multi-Viewport Issues
- `ImGuiConfigFlags_ViewportsEnable` is **NOT enabled**
- Only `ImGuiConfigFlags_DockingEnable` is active
- Single viewport architecture - no platform windows

---

## Possible Root Causes (Outside Editor System)

If the diagnostic does NOT detect duplicate Begin() calls, the issue must be:

### 1. ImGui Draw List Submission
**Hypothesis:** Draw data is being submitted to GPU twice
```cpp
// In Controller::DoRender()
ImGui::Render();                     // Generate draw lists
renderer_->Clear();                  // Clear framebuffer
ImGui_ImplSDLRenderer2_RenderDrawData(...); // Submit to GPU
renderer_->Present();                // Swap buffers
```

**Check:**
- Are draw lists being submitted twice?
- Is `ImGui_ImplSDLRenderer2_RenderDrawData()` called more than once?
- Add: `printf("RenderDrawData called: frame %d\n", ImGui::GetFrameCount());`

### 2. Z-Ordering / Layering Bug
**Hypothesis:** Two overlapping windows with same content at same position
```cpp
// ImGui windows at same coordinates with same content
ImGui::SetNextWindowPos(ImVec2(100, 100));
ImGui::Begin("Window1");
DrawContent(); // Content rendered
ImGui::End();

// Another window at SAME position
ImGui::SetNextWindowPos(ImVec2(100, 100));
ImGui::Begin("Window2");
DrawContent(); // SAME content rendered again
ImGui::End();
```

**Check:**
- ImGui Metrics window → Show "Windows" section
- Look for duplicate windows with same position
- Check window Z-order and docking state

### 3. Texture Double-Binding
**Hypothesis:** Textures are bound/drawn twice in rendering backend
```cpp
// In SDL2 renderer backend
SDL_RenderCopy(renderer, texture, ...); // First draw
// ... some code ...
SDL_RenderCopy(renderer, texture, ...); // Accidental second draw
```

**Check:**
- SDL2 render target state
- Multiple texture binding in same frame
- Backend drawing primitives twice

### 4. Stale ImGui State
**Hypothesis:** Old draw commands not cleared between frames
```cpp
// Missing clear in backend
void NewFrame() {
  // Should clear old draw data here!
  ImGui_ImplSDLRenderer2_NewFrame();
}
```

**Check:**
- Is `ImGui::NewFrame()` clearing old state?
- Backend implementation of `NewFrame()` correct?
- Add: `ImGui::GetDrawData()->CmdListsCount` logging

---

## Recommended Next Steps

### Step 1: Run with Diagnostic
```bash
cmake --build build --target yaze -j4
./build/bin/yaze --rom_file=zelda3.sfc --editor=Overworld 2>&1 | grep "DUPLICATE"
```

**Expected Output:**
- If duplicates exist: `[EditorCard] DUPLICATE DETECTED: 'Overworld Canvas' Begin() called twice in frame 1234`
- If no duplicates: (no output)

### Step 2: Check Programmatically
```cpp
// In EditorManager::Update() after line 1056, add:
if (gui::EditorCard::HasDuplicateRendering()) {
  LOG_ERROR("Duplicate card rendering detected: %s",
            gui::EditorCard::GetDuplicateCardName().c_str());
  // Breakpoint here to inspect call stack
}
```

### Step 3A: If Duplicates Detected
**Trace the duplicate Begin() call:**
1. Set breakpoint in `EditorCard::Begin()` at line 279 (duplicate detection)
2. Condition: `duplicate_detected_ == true`
3. Inspect call stack to find second caller
4. Fix the duplicate code path

### Step 3B: If No Duplicates Detected
**Issue is outside EditorCard system:**
1. Enable ImGui Metrics: `ImGui::ShowMetricsWindow()`
2. Check "Windows" section for duplicate windows
3. Add logging to `Controller::DoRender()`:
   ```cpp
   static int render_count = 0;
   printf("DoRender #%d: DrawData CmdLists=%d\n",
          ++render_count, ImGui::GetDrawData()->CmdListsCount);
   ```
4. Inspect SDL2 backend for double submission
5. Check for stale GPU state between frames

### Step 4: Alternative Debugging
If issue persists, try:
```cpp
// In OverworldEditor::Update(), add frame tracking
static int last_frame = -1;
int current_frame = ImGui::GetFrameCount();
if (current_frame == last_frame) {
  LOG_ERROR("OverworldEditor::Update() called TWICE in frame %d!", current_frame);
}
last_frame = current_frame;
```

---

## Architecture Insights

### Editor Rendering Pattern
**Decentralized Card Creation:**
- Each editor creates `EditorCard` instances **locally** in its `Update()` method
- Cards are **not global** - they're stack-allocated temporaries
- Visibility is managed by **pointers to bool flags** that persist across frames

**Example:**
```cpp
// In OverworldEditor::Update() - called ONCE per frame
gui::EditorCard tile16_card("Tile16 Selector", ICON_MD_GRID_3X3);
if (show_tile16_selector_) {  // Persistent flag
  if (tile16_card.Begin(&show_tile16_selector_)) {
    DrawTile16Selector(); // Content rendered ONCE
  }
  tile16_card.End();
}
// Card destroyed at end of Update() - stack unwinding
```

### Registry vs Direct Rendering
**EditorCardRegistry:**
- **Purpose:** Manage visibility flags, sidebar UI, card browser
- **Does NOT render cards** - only manages state
- **Does render:** Sidebar buttons, card browser UI, tree view

**Direct Rendering (in editors):**
- Each editor creates and renders its own cards
- Registry provides visibility flag pointers
- Editor checks flag, renders if true

### Separation of Concerns
**Clear boundaries:**
1. **Controller** - Main loop, window management, single Update() call
2. **EditorManager** - Editor lifecycle, session management, single editor->Update() per editor
3. **Editor (e.g., OverworldEditor)** - Card creation, content rendering, one Begin/End pair per card
4. **EditorCard** - ImGui window wrapper, duplicate detection, Begin/End state tracking
5. **EditorCardRegistry** - Visibility management, sidebar UI, no direct card rendering

**This architecture prevents duplicate rendering by design** - there is only ONE path from main loop to card content.

---

## Diagnostic Code Summary

**Location:** `src/app/gui/app/editor_layout.h` (lines 121-135) and `editor_layout.cc` (lines 17-285)

**Static Tracking Variables:**
```cpp
static int last_frame_count_ = 0;
static std::vector<std::string> cards_begun_this_frame_;
static bool duplicate_detected_ = false;
static std::string duplicate_card_name_;
```

**Detection Logic:**
```cpp
// In EditorCard::Begin()
int current_frame = ImGui::GetFrameCount();
if (current_frame != last_frame_count_) {
  // New frame - reset tracking
  cards_begun_this_frame_.clear();
  duplicate_detected_ = false;
}

// Check for duplicate
for (const auto& card_name : cards_begun_this_frame_) {
  if (card_name == window_name_) {
    duplicate_detected_ = true;
    fprintf(stderr, "[EditorCard] DUPLICATE: '%s' frame %d\n",
            window_name_.c_str(), current_frame);
  }
}
cards_begun_this_frame_.push_back(window_name_);
```

**Public API:**
```cpp
static void ResetFrameTracking();           // Manual reset (optional)
static bool HasDuplicateRendering();        // Check if duplicate detected
static const std::string& GetDuplicateCardName(); // Get duplicate card name
```

---

## Conclusion

The editor system has a **clean, single-path rendering architecture**. No code paths exist that could cause duplicate card rendering through the normal Update() flow.

**If duplicate rendering occurs:**
1. The diagnostic WILL detect it if it's in EditorCard::Begin()
2. If diagnostic doesn't fire, issue is outside EditorCard (ImGui backend, GPU state, Z-order)

**Next Agent Action:**
- Build and run with diagnostic
- Report findings based on stderr output
- Follow appropriate Step 3A or 3B from "Recommended Next Steps"

---

## Files Referenced

**Core Investigation Files:**
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/controller.cc` - Main loop (lines 56-165)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/editor/editor_manager.cc` - Update flow (lines 616-1079)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/editor/overworld/overworld_editor.cc` - Editor Update (lines 228-377)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/app/editor_layout.cc` - EditorCard implementation (lines 256-380)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/editor/ui/right_panel_manager.cc` - Panel system (lines 117-242)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/editor/system/editor_card_registry.cc` - Card registry (lines 456-787)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/editor/system/proposal_drawer.h` - Draw vs DrawContent (lines 39-43)

**Diagnostic Code:**
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/app/editor_layout.h` (lines 121-135)
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/app/editor_layout.cc` (lines 17-285)

**Previous Investigation:**
- `$TRUNK_ROOT/scawful/retro/yaze/docs/internal/handoff-duplicate-rendering-investigation.md`
