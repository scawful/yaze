# Editor UI Architecture Refactor Proposal

## Executive Summary

This document proposes a refactoring of yaze's editor panel system to eliminate naming confusion, centralize lifecycle management, and reduce boilerplate. The key change is **renaming "Card" terminology to "Panel"**—a more precise term used by professional IDEs like VSCode, JetBrains, and Xcode.

---

## Part 1: Terminology Decision

### Why "Panel" Instead of "Card"?

| Term | Associations | Precision |
|:-----|:-------------|:----------|
| **Card** | Material Design cards, Kanban boards, mobile UI | Vague—cards are typically static content containers |
| **Panel** | VSCode Side Panel, JetBrains Tool Windows, Xcode Inspectors | Precise—panels are dockable, resizable tool windows |
| **Pane** | Split views, editor areas | Typically refers to divisions within a window |
| **Tool Window** | JetBrains, Visual Studio | Verbose, but very precise |

**Decision**: Use **"Panel"** as the primary term:
- `gui::EditorCard` → `gui::PanelWindow`
- `CardInfo` → `PanelDescriptor`
- `EditorCardRegistry` → `PanelManager`
- `*Card` classes → `*Panel` classes

---

## Part 2: Current Architecture Analysis

### 2.1 Key Components (As-Is)

| Component | Location | Current Name | Proposed Name |
|:----------|:---------|:-------------|:--------------|
| ImGui window wrapper | `editor_layout.h` | `gui::EditorCard` | `gui::PanelWindow` |
| Metadata struct | `editor_card_registry.h` | `CardInfo` | `PanelDescriptor` |
| Central registry | `editor_card_registry.h` | `EditorCardRegistry` | `PanelManager` |
| Sidebar UI | `activity_bar.h` | `ActivityBar` | `ActivityBar` (unchanged) |
| Layout builder | `layout_manager.h` | `LayoutManager` | `LayoutManager` (unchanged) |
| Presets | `layout_presets.h` | `LayoutPresets` | `LayoutPresets` (unchanged) |

### 2.2 Current Workflow (Dual Registration Problem)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CURRENT WORKFLOW                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Editor::Initialize()                    Editor::Update()                    │
│  ───────────────────                     ─────────────────                   │
│                                                                              │
│  1. Register CardInfo metadata           1. Create gui::EditorCard instance  │
│     with registry                           (per-frame or static)            │
│                                                                              │
│  card_registry->RegisterCard({           gui::EditorCard my_panel(           │
│    .card_id = MakeCardId("x.foo"),         MakeCardTitle("Foo"),             │
│    .display_name = "Foo",                  ICON_MD_FOO);                     │
│    .window_title = " Foo",                                                   │
│    .visibility_flag = &show_foo_,        if (my_panel.Begin(&show_foo_)) {   │
│    ...                                     DrawFooContent();                 │
│  });                                     }                                   │
│                                          my_panel.End();                     │
│                                                                              │
│  PROBLEM: Two separate steps, titles can diverge, no centralized drawing    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Identified Issues

1. **Naming Collision**: `gui::EditorCard` vs conceptual "editor card" vs `*Card` classes
2. **Dual Registration**: Editors register metadata AND manually draw—no central control
3. **Title Mismatch Risk**: `CardInfo.window_title` vs `gui::EditorCard` constructor title
4. **No Cross-Editor Panel Support**: Panels are tied to their parent editor
5. **Inconsistent Instantiation**: Static vs per-frame vs unique_ptr member patterns

---

## Part 3: Complete Editor Inventory

### 3.1 Overworld Editor

#### Tool Panels (Static)
| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `overworld.canvas` | Overworld Canvas | MAP | Main map editing canvas with toolset | ✅ |
| `overworld.tile16_selector` | Tile16 Selector | GRID_ON | Tile palette for painting | ✅ |
| `overworld.tile8_selector` | Tile8 Selector | GRID_3X3 | Low-level 8x8 tile editing | ❌ |
| `overworld.area_graphics` | Area Graphics | IMAGE | GFX sheet preview for current area | ✅ |
| `overworld.scratch` | Scratch Workspace | DRAW | Layout planning/clipboard area | ❌ |
| `overworld.gfx_groups` | GFX Groups | FOLDER | Graphics group configuration | ❌ |
| `overworld.usage_stats` | Usage Statistics | ANALYTICS | Tile usage analysis across all maps | ✅ |
| `overworld.v3_settings` | v3 Settings | SETTINGS | ZSCustomOverworld configuration | ❌ |
| `overworld.properties` | Map Properties | TUNE | Per-map settings (palette, GFX, etc.) | ✅ |
| `overworld.debug` | Debug Window | BUG_REPORT | Internal debug information | ❌ |

#### Resource Panels (Dynamic per-map)
| Panel ID Pattern | Display Name | Purpose |
|:-----------------|:-------------|:--------|
| `overworld.map_{id}` | Map {id} ({world}) | Focused view of specific overworld map |
| `overworld.map_{id}.entities` | Map {id} Entities | Entity list (entrances, exits, items, sprites) |

**Note**: `overworld.usage_stats` is useful cross-editor—see Section 5.

### 3.2 Dungeon Editor (V2)

#### Tool Panels (Static)
| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `dungeon.control_panel` | Dungeon Controls | CASTLE | Mode and tool selection | ✅ |
| `dungeon.room_selector` | Room Selector | LIST | Room list with search/filter (296 rooms) | ✅ |
| `dungeon.entrance_list` | Entrance List | DOOR_FRONT | Entrance list with search/filter | ✅ |
| `dungeon.room_matrix` | Room Matrix | GRID_VIEW | Visual 16x16 room layout | ✅ |
| `dungeon.entrances` | Entrance Properties | DOOR_SLIDING | Selected entrance property editor | ❌ |
| `dungeon.room_graphics` | Room Graphics | IMAGE | Room GFX sheet preview | ✅ |
| `dungeon.object_editor` | Object Editor | CONSTRUCTION | Object placement/editing | ✅ |
| `dungeon.palette_editor` | Palette Editor | PALETTE | Room palette selection | ❌ |
| `dungeon.debug_controls` | Debug Controls | BUG_REPORT | Debug tools and state inspection | ❌ |
| `dungeon.emulator_preview` | SNES Object Preview | MONITOR | Live emulator object preview | ❌ |

#### Resource Panels (Dynamic per-room)
| Panel ID Pattern | Display Name | Purpose |
|:-----------------|:-------------|:--------|
| `dungeon.room_{id}` | Room {id} | Canvas for editing specific room (0-295) |
| `dungeon.room_{id}.objects` | Room {id} Objects | Object list for specific room |

**Architecture**: Dungeon Editor is pure panel-based—no "main canvas" concept. All panels are peers. **Resource panels** (room-specific) are created on-demand when a room is opened.

### 3.3 Graphics Editor

#### Tool Panels (Static)
| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `graphics.sheet_browser_v2` | Sheet Browser | VIEW_LIST | Navigate all 223 graphics sheets | ✅ |
| `graphics.pixel_editor` | Pixel Editor | DRAW | 8x8/16x16 tile pixel editing | ✅ |
| `graphics.palette_controls` | Palette Controls | PALETTE | Palette selection for editing | ✅ |
| `graphics.link_sprite_editor` | Link Sprite Editor | PERSON | Edit Link's sprite frames | ❌ |
| `graphics.polyhedral_editor` | 3D Objects | VIEW_IN_AR | Edit rupees, crystals, triforce | ✅ |
| `graphics.sheet_editor` | Sheet Editor (Legacy) | EDIT | Older sheet editing interface | ❌ |
| `graphics.sheet_browser` | Asset Browser (Legacy) | VIEW_LIST | Older asset browsing | ❌ |
| `graphics.player_animations` | Player Animations | PERSON | View Link animation sequences | ❌ |
| `graphics.prototype_viewer` | Prototype Viewer | CONSTRUCTION | Experimental feature viewer | ❌ |

#### Resource Panels (Dynamic per-sheet)
| Panel ID Pattern | Display Name | Purpose |
|:-----------------|:-------------|:--------|
| `graphics.sheet_{id}` | Sheet {id} | Dedicated editor for specific GFX sheet |

### 3.4 Palette Editor

TODO: Remove control panel and fold into activity bar for useful actions? Resource management

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `palette.control_panel` | Palette Controls | PALETTE | Group manager and quick actions | ✅ |
| `palette.ow_main` | Overworld Main | LANDSCAPE | 6 overworld area palettes | ✅ |
| `palette.ow_animated` | Overworld Animated | WATER | Water, lava animation palettes | ❌ |
| `palette.dungeon_main` | Dungeon Main | CASTLE | 20 dungeon room palettes | ✅ |
| `palette.sprites` | Global Sprite Palettes | PETS | Main sprite color sets | ✅ |
| `palette.sprites_aux1` | Sprites Aux 1 | FILTER_1 | Auxiliary sprite colors 1 | ❌ |
| `palette.sprites_aux2` | Sprites Aux 2 | FILTER_2 | Auxiliary sprite colors 2 | ❌ |
| `palette.sprites_aux3` | Sprites Aux 3 | FILTER_3 | Auxiliary sprite colors 3 | ❌ |
| `palette.equipment` | Equipment Palettes | SHIELD | Link's tunic/equipment colors | ❌ |
| `palette.quick_access` | Quick Access | COLOR_LENS | Color harmony tools | ✅ |
| `palette.custom` | Custom Palette | BRUSH | Custom palette editing | ❌ |

### 3.5 Music Editor

#### Tool Panels (Static)
| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `music.song_browser` | Song Browser | LIBRARY_MUSIC | Navigate all game songs | ✅ |
| `music.tracker` | Playback Control | PLAY_CIRCLE | Transport controls and BPM | ✅ |
| `music.piano_roll` | Piano Roll | PIANO | Visual note editing | ❌ |
| `music.instrument_editor` | Instrument Editor | SPEAKER | Edit instrument samples | ✅ |
| `music.sample_editor` | Sample Editor | WAVES | Edit BRR audio samples | ❌ |
| `music.assembly` | Assembly View | CODE | View music as 65816 assembly | ❌ |
| `music.help` | Help | HELP | Music editor documentation | ❌ |

#### Resource Panels (Dynamic per-song)
| Panel ID Pattern | Display Name | Purpose |
|:-----------------|:-------------|:--------|
| `music.song_{index}` | Song: {name} | Full song editor for a specific track |
| `music.song_{index}.piano_roll` | {name} Piano Roll | Piano roll for specific song |
| `music.song_{index}.channels` | {name} Channels | Channel mixer for specific song |

**Dynamic Panels**: Music Editor creates per-song panels on demand. Multiple songs can be open simultaneously for comparison/copying.

### 3.6 Screen Editor

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `screen.dungeon_maps` | Dungeon Maps | MAP | Edit dungeon map screens | ✅ |
| `screen.inventory_menu` | Inventory Menu | INVENTORY | Edit inventory/pause menu | ✅ |
| `screen.overworld_map` | Overworld Map | PUBLIC | Edit overworld map screen | ❌ |
| `screen.title_screen` | Title Screen | TITLE | Edit title screen graphics | ✅ |
| `screen.naming_screen` | Naming Screen | EDIT | Edit save file naming screen | ❌ |

### 3.7 Sprite Editor

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `sprite.vanilla_editor` | Vanilla Sprites | SMART_TOY | View/edit built-in sprites | ✅ |
| `sprite.custom_editor` | Custom Sprites | ADD_CIRCLE | Import/edit custom ZSM sprites | ❌ |

### 3.8 Message Editor

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `message.message_list` | Message List | LIST | Navigate all game messages | ✅ |
| `message.message_editor` | Message Editor | EDIT | Edit message text and formatting | ✅ |
| `message.font_atlas` | Font Atlas | FONT_DOWNLOAD | View/edit font graphics | ✅ |
| `message.dictionary` | Dictionary | BOOK | Edit compression dictionary | ❌ |

### 3.9 Assembly Editor

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `assembly.editor` | Assembly Editor | CODE | 65816 assembly code editor | ✅ |
| `assembly.file_browser` | File Browser | FOLDER_OPEN | Navigate ASM project files | ✅ |

**Note**: Assembly Editor uses file browser integration for project navigation.

### 3.10 Agent Editor

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `agent.chat` | Agent Chat | CHAT | Main AI chat interface | ✅ |
| `agent.configuration` | AI Configuration | SETTINGS | API keys, model selection | ❌ |
| `agent.status` | Agent Status | INFO | Connection status, context info | ❌ |
| `agent.prompt_editor` | Prompt Editor | EDIT | Edit system prompts | ❌ |
| `agent.profiles` | Bot Profiles | FOLDER | Manage bot personalities | ❌ |
| `agent.history` | Chat History | HISTORY | View past conversations | ❌ |
| `agent.metrics` | Metrics Dashboard | ANALYTICS | Token usage, response times | ❌ |
| `agent.builder` | Agent Builder | AUTO_FIX_HIGH | Create custom agents | ❌ |

### 3.11 Emulator (Registered by EditorManager)

TODO: Consolidate some panels into PpuViewer nav as a canvas of sorts

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `emulator.cpu_debugger` | CPU Debugger | BUG_REPORT | CPU registers, stepping | ✅ |
| `emulator.ppu_viewer` | PPU Viewer | VIDEOGAME_ASSET | Graphics layer debugging | ✅ |
| `emulator.memory_viewer` | Memory Viewer | MEMORY | RAM/VRAM inspection | ❌ |
| `emulator.breakpoints` | Breakpoints | STOP | Breakpoint management | ❌ |
| `emulator.performance` | Performance | SPEED | Frame timing, FPS | ✅ |
| `emulator.ai_agent` | AI Agent | SMART_TOY | AI-assisted debugging | ❌ |
| `emulator.save_states` | Save States | SAVE | Save/load state management | ✅ |
| `emulator.keyboard_config` | Keyboard Config | KEYBOARD | Input configuration | ✅ |
| `emulator.virtual_controller` | Virtual Controller | SPORTS_ESPORTS | On-screen gamepad | ✅ |
| `emulator.apu_debugger` | APU Debugger | AUDIOTRACK | Audio processor debugging | ❌ |
| `emulator.audio_mixer` | Audio Mixer | AUDIO_FILE | Channel volume control | ❌ |

### 3.12 Memory (Registered by EditorManager)

TODO: Add more panels to help with hex editing operations?

| Panel ID | Display Name | Icon | Purpose | Default Visible |
|:---------|:-------------|:-----|:--------|:----------------|
| `memory.hex_editor` | Hex Editor | MEMORY | Raw ROM hex editing | ✅ |

---

## Part 4: Cross-Editor Panel Visibility

### 4.1 The Problem

Currently, panels are tightly coupled to their parent editor. When switching editors, panels are hidden. But some panels are useful across editors:

| Panel | Useful When... |
|:------|:---------------|
| `overworld.usage_stats` | Editing dungeons to check tile availability |
| `palette.ow_main` | Editing overworld tiles to preview palette effects |
| `emulator.cpu_debugger` | Any editing to test changes in real-time |
| `graphics.pixel_editor` | Editing sprites, dungeons, or messages |

### 4.2 Panel Categories

We propose three panel categories based on lifecycle behavior:

| Category | Behavior | Examples |
|:---------|:---------|:---------|
| **Editor-Bound** | Hidden when switching away from parent editor | `dungeon.room_selector`, `music.piano_roll` |
| **Persistent** | Remains visible across editor switches | `emulator.cpu_debugger`, `palette.quick_access` |
| **Cross-Editor** | Can be pinned to stay visible (user choice) | `overworld.usage_stats`, `graphics.pixel_editor` |

### 4.3 Implementation: Pin-to-Persist

Add a "pin" button to panel headers:

```cpp
class PanelWindow {
 public:
  // ... existing methods ...
  
  /// If pinned, panel stays visible when switching editors
  void SetPinned(bool pinned) { pinned_ = pinned; }
  bool IsPinned() const { return pinned_; }
  
 private:
  bool pinned_ = false;
};
```

The `PanelManager` respects pinned state:

```cpp
void PanelManager::OnEditorSwitch(EditorType from, EditorType to) {
  // Hide non-pinned panels from the previous editor
  for (auto& [id, panel] : panels_) {
    if (panel->GetCategory() == GetCategoryForEditor(from) && !IsPinned(id)) {
      HidePanel(id);
    }
  }
  
  // Show default panels for new editor
  auto defaults = LayoutPresets::GetDefaultPanels(to);
  for (const auto& id : defaults) {
    ShowPanel(id);
  }
}
```

### 4.4 Related Panel Cascade (Optional)

For **Editor-Bound** panels only, we can define parent-child relationships:

```cpp
struct PanelDescriptor {
  // ... existing fields ...
  
  /// If set, this panel closes when parent panel closes
  std::string parent_panel_id;
  
  /// If true, closing this panel also closes children
  bool cascade_close = false;
};
```

**Example**: Closing `dungeon.control_panel` could cascade-close `dungeon.object_editor` if they're defined as related.

**Documentation Requirement**: Any cascade behavior MUST be documented in `LayoutPresets` comments.

---

## Part 5: Resource Panels & Multi-Session Support

### 5.1 The Resource Panel Concept

Some panels represent specific **resources** within the ROM—not generic tools, but windows for editing a particular piece of data:

| Editor | Resource Type | Example Panel IDs |
|:-------|:--------------|:------------------|
| Dungeon | Rooms (0-295) | `dungeon.room_42`, `dungeon.room_128` |
| Music | Songs (0-63) | `music.song_5`, `music.song_12` |
| Overworld | Maps (0-159) | `overworld.map_0`, `overworld.map_64` |
| Graphics | Sheets (0-222) | `graphics.sheet_100`, `graphics.sheet_212` |
| Sprite | Sprites (0-255) | `sprite.vanilla_42`, `sprite.custom_3` |
| Message | Messages (0-395) | `message.text_42`, `message.text_100` |

### 5.2 Resource Panel Lifecycle

```cpp
/// Base class for resource-specific panels
class ResourcePanel : public EditorPanel {
 public:
  /// The resource ID this panel edits (room_id, song_index, etc.)
  virtual int GetResourceId() const = 0;
  
  /// Resource type for grouping
  virtual std::string GetResourceType() const = 0;
  
  /// Can have multiple instances open simultaneously
  virtual bool AllowMultipleInstances() const { return true; }
  
  // Resource panels are always EditorBound by default
  PanelCategory GetPanelCategory() const override { 
    return PanelCategory::EditorBound; 
  }
};
```

### 5.3 Resource Panel ID Format

```
{session}.{category}.{resource_type}_{resource_id}[.{subpanel}]

Examples:
  s0.dungeon.room_42           -- Room 42 in session 0
  s1.dungeon.room_42           -- Room 42 in session 1 (different ROM)
  s0.music.song_5              -- Song 5 in session 0
  s0.music.song_5.piano_roll   -- Piano roll for song 5
  s0.overworld.map_64.entities -- Entity list for map 64
```

### 5.4 Multi-Session Awareness

When multiple ROMs are loaded (multi-session editing), resource panels must be uniquely identified:

```cpp
class PanelManager {
 public:
  /// Create a resource panel for the current session
  std::string CreateResourcePanel(const std::string& category,
                                  const std::string& resource_type,
                                  int resource_id) {
    std::string panel_id = MakeResourcePanelId(
        active_session_, category, resource_type, resource_id);
    
    // Check if already exists
    if (panels_.contains(panel_id)) {
      ShowPanel(panel_id);  // Just bring to front
      return panel_id;
    }
    
    // Create new resource panel
    auto panel = CreateResourcePanelImpl(category, resource_type, resource_id);
    RegisterPanel(std::move(panel));
    ShowPanel(panel_id);
    
    return panel_id;
  }
  
  /// Generate session-aware resource panel ID
  std::string MakeResourcePanelId(size_t session_id,
                                  const std::string& category,
                                  const std::string& resource_type,
                                  int resource_id) const {
    if (session_count_ > 1) {
      return absl::StrFormat("s%zu.%s.%s_%d", 
                             session_id, category, resource_type, resource_id);
    }
    return absl::StrFormat("%s.%s_%d", category, resource_type, resource_id);
  }
  
  /// Get all resource panels for a session
  std::vector<EditorPanel*> GetResourcePanelsInSession(size_t session_id);
  
  /// Close all resource panels when a session closes
  void CloseSessionResourcePanels(size_t session_id);
};
```

### 5.5 Multi-ROM Side-by-Side Editing

With session-aware IDs, users can:

1. **Open two ROMs** (vanilla + hack)
2. **View same room side-by-side**:
   - `s0.dungeon.room_42` (vanilla)
   - `s1.dungeon.room_42` (hack)
3. **Compare and copy** between them

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      MULTI-SESSION RESOURCE PANELS                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Session 0 (vanilla.sfc)              Session 1 (myhack.sfc)                │
│  ┌─────────────────────┐              ┌─────────────────────┐               │
│  │ s0.dungeon.room_42  │              │ s1.dungeon.room_42  │               │
│  │ Room 42             │              │ Room 42             │               │
│  │ [Session 0]         │   ◄─────►    │ [Session 1]         │               │
│  │                     │   Compare    │                     │               │
│  │ ┌───────────────┐   │              │ ┌───────────────┐   │               │
│  │ │ Canvas        │   │              │ │ Canvas        │   │               │
│  │ │ (vanilla)     │   │              │ │ (modified)    │   │               │
│  │ └───────────────┘   │              │ └───────────────┘   │               │
│  └─────────────────────┘              └─────────────────────┘               │
│                                                                              │
│  Window Title Format: "Room 42 [S0]"  "Room 42 [S1]"                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 5.6 Resource Panel Window Titles

```cpp
std::string PanelManager::GetResourcePanelTitle(const std::string& panel_id) const {
  auto* panel = GetPanel(panel_id);
  if (!panel) return "";
  
  auto* resource_panel = dynamic_cast<ResourcePanel*>(panel);
  if (!resource_panel) {
    return GetWindowTitle(panel_id);  // Fallback to normal
  }
  
  std::string title = absl::StrFormat("%s %s %d",
      resource_panel->GetIcon(),
      resource_panel->GetResourceType(),
      resource_panel->GetResourceId());
  
  // Add session suffix for multi-session
  if (session_count_ > 1) {
    size_t session_id = GetSessionFromPanelId(panel_id);
    title += absl::StrFormat(" [S%zu]", session_id);
  }
  
  return title;
}
```

### 5.7 Resource Panel Limits

To prevent memory bloat, enforce limits:

```cpp
struct ResourcePanelLimits {
  static constexpr size_t kMaxRoomPanels = 8;      // Max open rooms
  static constexpr size_t kMaxSongPanels = 4;      // Max open songs
  static constexpr size_t kMaxSheetPanels = 6;     // Max open GFX sheets
  static constexpr size_t kMaxTotalResourcePanels = 20;
};
```

When limit is reached:
1. **Option A**: Auto-close oldest panel (LRU)
2. **Option B**: Warn user and prevent new panel
3. **Option C**: Prompt to close one (user choice)

**Recommendation**: Option A with undo (can reopen from sidebar).

---

## Part 6: Proposed Architecture

### 6.1 New Terminology Mapping

| Current | Proposed | Description |
|:--------|:---------|:------------|
| `gui::EditorCard` | `gui::PanelWindow` | Pure ImGui window wrapper |
| `CardInfo` | `PanelDescriptor` | Panel metadata (immutable after registration) |
| `EditorCardRegistry` | `PanelManager` | Owns panels, manages lifecycle, draws all |
| `*Card` classes | `*Panel` classes | Logical component implementations |

### 6.2 Interface Definitions

#### `gui::PanelWindow`

```cpp
namespace yaze::gui {

/// Pure ImGui window wrapper for dockable panels
class PanelWindow {
 public:
  enum class Position { Free, Left, Right, Bottom, Top, Floating };

  PanelWindow(const char* title, const char* icon = nullptr);

  // Configuration
  void SetDefaultSize(float width, float height);
  void SetPosition(Position pos);
  void SetMinimizable(bool minimizable);
  void SetClosable(bool closable);
  void SetDockingAllowed(bool allowed);
  void SetPinnable(bool pinnable);  // NEW: Allow pin-to-persist

  // Header buttons (drawn in title bar)
  void AddHeaderButton(const char* icon, const char* tooltip,
                       std::function<void()> callback);

  // ImGui lifecycle
  bool Begin(bool* p_open = nullptr);
  void End();

  // State
  void SetPinned(bool pinned);
  bool IsPinned() const;
  void Focus();
  bool IsFocused() const;
  const char* GetWindowName() const;
};

}  // namespace yaze::gui
```

#### `editor::EditorPanel` (New Interface)

```cpp
namespace yaze::editor {

/// Category for panel lifecycle behavior
enum class PanelCategory {
  EditorBound,  // Hidden on editor switch (default)
  Persistent,   // Always visible once shown
  CrossEditor   // User can pin to persist
};

/// Base interface for all logical panel components
class EditorPanel {
 public:
  virtual ~EditorPanel() = default;

  // ========== Identity ==========
  virtual std::string GetId() const = 0;           // e.g., "dungeon.room_selector"
  virtual std::string GetDisplayName() const = 0;  // e.g., "Room Selector"
  virtual std::string GetIcon() const = 0;         // e.g., ICON_MD_LIST
  virtual std::string GetEditorCategory() const = 0;  // e.g., "Dungeon"

  // ========== Drawing ==========
  virtual void Draw(bool* p_open) = 0;  // Called when visible

  // ========== Lifecycle ==========
  virtual void OnOpen() {}   // Panel becomes visible
  virtual void OnClose() {}  // Panel becomes hidden
  virtual void OnFocus() {}  // Panel receives focus

  // ========== Behavior ==========
  virtual PanelCategory GetPanelCategory() const { 
    return PanelCategory::EditorBound; 
  }
  virtual bool IsEnabled() const { return true; }
  virtual std::string GetDisabledTooltip() const { return ""; }
  virtual std::string GetShortcutHint() const { return ""; }
  virtual int GetPriority() const { return 50; }
  
  // ========== Relationships ==========
  virtual std::string GetParentPanelId() const { return ""; }
  virtual bool CascadeCloseChildren() const { return false; }
};

}  // namespace yaze::editor
```

#### `editor::PanelManager`

```cpp
namespace yaze::editor {

/// Central manager for all EditorPanel instances
class PanelManager {
 public:
  // ========== Registration ==========
  void RegisterPanel(std::unique_ptr<EditorPanel> panel);
  
  template <typename T, typename... Args>
  T* EmplacePanel(Args&&... args);
  
  void UnregisterPanel(const std::string& panel_id);

  // ========== Visibility ==========
  void ShowPanel(const std::string& panel_id);
  void HidePanel(const std::string& panel_id);
  void TogglePanel(const std::string& panel_id);
  bool IsPanelVisible(const std::string& panel_id) const;
  
  void ShowAllInCategory(const std::string& category);
  void HideAllInCategory(const std::string& category);

  // ========== Central Drawing ==========
  void DrawAllVisiblePanels();  // Call once per frame

  // ========== Editor Switching ==========
  void OnEditorSwitch(EditorType from, EditorType to);
  void SetActiveEditor(EditorType type);

  // ========== Pin Management ==========
  void SetPanelPinned(const std::string& panel_id, bool pinned);
  bool IsPanelPinned(const std::string& panel_id) const;
  std::vector<std::string> GetPinnedPanels() const;

  // ========== Query ==========
  EditorPanel* GetPanel(const std::string& panel_id);
  std::vector<EditorPanel*> GetPanelsInCategory(const std::string& category);
  std::vector<std::string> GetAllCategories() const;

  // ========== Session Support ==========
  void SetActiveSession(size_t session_id);
  size_t GetActiveSession() const;

  // ========== Persistence ==========
  void SaveVisibilityState();
  void LoadVisibilityState();
  void SavePinnedState();
  void LoadPinnedState();

 private:
  std::unordered_map<std::string, std::unique_ptr<EditorPanel>> panels_;
  std::unordered_map<std::string, bool> visibility_;
  std::unordered_map<std::string, bool> pinned_;
  EditorType active_editor_ = EditorType::kUnknown;
  size_t active_session_ = 0;
};

}  // namespace yaze::editor
```

### 6.3 Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        PROPOSED ARCHITECTURE                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                          PanelManager                                    │ │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │ │
│  │  │ panels_: map<string, unique_ptr<EditorPanel>>                        │ │ │
│  │  │                                                                      │ │ │
│  │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │ │ │
│  │  │  │ RoomList     │  │ ObjectEditor │  │  UsageStats  │  ...          │ │ │
│  │  │  │    Panel     │  │    Panel     │  │    Panel     │               │ │ │
│  │  │  │ [EditorBound]│  │ [EditorBound]│  │ [CrossEditor]│               │ │ │
│  │  │  └──────────────┘  └──────────────┘  └──────────────┘               │ │ │
│  │  └─────────────────────────────────────────────────────────────────────┘ │ │
│  │                                                                          │ │
│  │  DrawAllVisiblePanels() {                                                │ │
│  │    for (auto& [id, panel] : panels_) {                                   │ │
│  │      if (!IsVisible(id)) continue;                                       │ │
│  │                                                                          │ │
│  │      gui::PanelWindow window(GetWindowTitle(id), panel->GetIcon());      │ │
│  │      if (IsPinnable(id)) window.SetPinnable(true);                       │ │
│  │                                                                          │ │
│  │      if (window.Begin(&visibility_[id])) {                               │ │
│  │        panel->Draw(&visibility_[id]);                                    │ │
│  │      }                                                                   │ │
│  │      window.End();                                                       │ │
│  │    }                                                                     │ │
│  │  }                                                                       │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
│  ┌──────────────┐     ┌───────────────┐     ┌───────────────────┐           │
│  │ ActivityBar  │────▶│  PanelManager │◀────│   LayoutManager   │           │
│  │ (Sidebar UI) │     │  (Ownership)  │     │ (DockBuilder)     │           │
│  └──────────────┘     └───────────────┘     └───────────────────┘           │
│                              │                                               │
│                              ▼                                               │
│                       ┌──────────────┐                                       │
│                       │LayoutPresets │                                       │
│                       │(Default vis) │                                       │
│                       └──────────────┘                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Part 7: Migration Plan & Current Status

### Current Status (December 2024)

**✅ PHASES 1-8 COMPLETE** - Full panel system migration complete including resource panels and cross-editor features.

| Phase | Status | Description |
|:------|:-------|:------------|
| Phase 1 | ✅ Complete | UI widget renamed (`EditorCard` → `PanelWindow`) |
| Phase 2 | ✅ Complete | Registry renamed (`EditorCardRegistry` deleted, `PanelManager` created) |
| Phase 3 | ✅ Complete | `EditorPanel` interface defined with central drawing |
| Phase 4 | ✅ Complete | Dungeon Editor migrated (9 panels) |
| Phase 5 | ✅ Complete | All remaining editors migrated (40 panels) |
| Phase 6 | ✅ Complete | Resource panels with LRU logic |
| Phase 7 | ✅ Complete | Cross-editor features (Pin-to-Persist, OnEditorSwitch) |
| Phase 8 | ✅ Complete | Multi-session verification and testing |

### Phase 5 Completion Details

All editors now use the EditorPanel architecture:

| Editor | Static Panels | Resource Panels | Status | Pattern Used |
|:-------|:--------------|:----------------|:-------|:-------------|
| **Dungeon Editor** | 10 | `dungeon.room_{id}` | ✅ Complete | Callback + Resource |
| **Graphics Editor** | 5 | — | ✅ Complete | Direct interface |
| **Music Editor** | 7 | `music.song_{id}`, `music.piano_roll_{id}` | ✅ Complete | Callback + Resource |
| **Palette Editor** | 11 | — | ✅ Complete | Callback pattern |
| **Agent Editor** | 8 | — | ✅ Complete | Callback pattern |
| **Sprite Editor** | 2 | — | ✅ Complete | Callback pattern |
| **Screen Editor** | 5 | — | ✅ Complete | Callback pattern |
| **Message Editor** | 4 | — | ✅ Complete | Callback pattern |
| **Overworld Editor** | 9 | — | ✅ Complete | Direct pointer pattern |

**Total: 61 static EditorPanel implementations + dynamic resource panels**

### Overworld Editor Migration Details

The Overworld Editor migration was the most complex, using a **direct pointer pattern** with separate `.cc` files for maintainability:

#### Panels Created (9 total)

**High-Priority Panels:**
| Panel | ID | Purpose | File |
|:------|:---|:--------|:-----|
| AreaGraphicsPanel | `overworld.area_graphics` | GFX sheet preview for current area | `area_graphics_panel.h/.cc` |
| Tile16SelectorPanel | `overworld.tile16_selector` | Tile palette for painting | `tile16_selector_panel.h/.cc` |
| MapPropertiesPanel | `overworld.properties` | Per-map settings editor | `map_properties_panel.h/.cc` |

**Medium-Priority Panels:**
| Panel | ID | Purpose | File |
|:------|:---|:--------|:-----|
| ScratchSpacePanel | `overworld.scratch` | Layout planning workspace | `scratch_space_panel.h/.cc` |
| UsageStatisticsPanel | `overworld.usage_stats` | Tile usage analysis | `usage_statistics_panel.h/.cc` |

**Low-Priority Panels:**
| Panel | ID | Purpose | File |
|:------|:---|:--------|:-----|
| Tile8SelectorPanel | `overworld.tile8_selector` | 8x8 tile editing | `tile8_selector_panel.h/.cc` |
| DebugWindowPanel | `overworld.debug` | Debug information | `debug_window_panel.h/.cc` |
| GfxGroupsPanel | `overworld.gfx_groups` | Graphics group configuration | `gfx_groups_panel.h/.cc` |
| V3SettingsPanel | `overworld.v3_settings` | ZSCustomOverworld settings | `v3_settings_panel.h/.cc` |

#### Architecture Pattern

```cpp
// Panel header (area_graphics_panel.h)
class AreaGraphicsPanel : public EditorPanel {
 public:
  explicit AreaGraphicsPanel(OverworldEditor* editor);

  std::string GetId() const override { return "overworld.area_graphics"; }
  std::string GetDisplayName() const override { return "Area Graphics"; }
  std::string GetIcon() const override { return ICON_MD_IMAGE; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

// Registration in overworld_editor.cc
void OverworldEditor::Initialize() {
  panel_manager->RegisterEditorPanel(
      std::make_unique<AreaGraphicsPanel>(this));
  // ... all 9 panels registered once
}
```

### Git Commits (16 atomic commits)

```
1cd667a93f docs: update architecture documentation for EditorPanel system
a436321bcc refactor(wasm): update control API for panel system compatibility
28ca469cb5 fix(emu): improve audio timing and SPC700 cycle accuracy
604667e47f feat(editor): integrate EditorManager and Activity Bar with panel system
060857179c refactor(ui): update settings and coordinator systems for panel architecture
e7c65a1c46 feat(overworld-editor): migrate to EditorPanel system
7df3980222 feat(message-editor): migrate to EditorPanel system
a91f08928a feat(screen-editor): migrate to EditorPanel system
081af10c80 feat(sprite-editor): migrate to EditorPanel system
36c8bee040 feat(agent-editor): migrate to EditorPanel system
862a69965d feat(palette-editor): migrate to EditorPanel system
d4f0dccf10 feat(music-editor): migrate to EditorPanel system
195cf8ac5e feat(graphics-editor): migrate existing panels to EditorPanel interface
a5daccce8e feat(dungeon-editor): migrate to EditorPanel system
e9e1123a57 refactor(layout): update layout system to use PanelManager
b652636618 refactor(editor): remove deprecated EditorCardRegistry
6007a1086d feat(editor): add EditorPanel system for unified panel management
```

### Benefits Achieved

- ✅ **Centralized Management** - All panels managed by `PanelManager`
- ✅ **Activity Bar Integration** - Panels automatically appear in sidebar
- ✅ **Efficient Rendering** - Panels created once in `Initialize()`, not per-frame
- ✅ **Layout Persistence** - Panel arrangements can be saved/restored
- ✅ **Session Support** - Proper scoping for multi-ROM editing
- ✅ **Consistent Architecture** - All editors follow same pattern

---

### Phase 6-8 Completion Details

### Phase 6: Resource Panels ✅ Complete

**Implemented Features:**
- `ResourcePanel` base class in `resource_panel.h`
- Dynamic room panels (`DungeonRoomPanel`) created on-demand when rooms selected
- Dynamic song panels (`MusicSongPanel`) and piano roll panels for music editor
- Session-aware ID generation via `MakePanelId()`
- LRU eviction in `EnforceResourceLimits()` respects pinned panels
- Resource panels appear in Activity Bar under their editor category
- Room selector properly limited to 296 rooms (kNumberOfRooms)

**Key Implementation Details:**
```cpp
// Resource panels use BASE IDs - PanelManager handles session prefixing
std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
panel_manager->RegisterPanel({.card_id = card_id, ...});
panel_manager->ShowPanel(card_id);  // Sets visibility immediately
```

**Important:** Do NOT use `MakeCardId()` for resource panels - it causes double-prefixing since `RegisterPanel()` already calls `MakePanelId()` internally.

### Phase 7: Cross-Editor Features ✅ Complete

**Implemented Features:**
1. **Pin-to-Persist UI** - Pin button in Activity Bar sidebar (not title bar due to ImGui limitations)
2. **`OnEditorSwitch()`** - Hides non-pinned panels from previous category, shows defaults for new
3. **Dashboard Category** - `kDashboardCategory` suppresses all panels until editor selected
4. **Category Filtering** - Resource panels only visible when their category is active OR pinned

**Key Implementation Details:**
```cpp
// In PanelManager::DrawAllVisiblePanels()
if (active_category_.empty() || active_category_ == kDashboardCategory) {
  return;  // Suppress panels when no editor selected
}

// Resource panels check category + pin status
if (panel->GetEditorCategory() == active_category_ || 
    IsPanelPinned(panel_id) ||
    panel->GetPanelCategory() == PanelCategory::Persistent) {
  // Draw panel
}
```

### Phase 8: Multi-Session Support ✅ Complete

**Verified Behaviors:**
- Session-aware panel IDs: `s0.dungeon.room_42` vs `s1.dungeon.room_42`
- Resource panels properly scoped to their session
- Fixed double-prefix bug (resource panels use base IDs, not `MakeCardId()`)
- Window titles include session suffix when multiple ROMs open

**Dungeon Editor Panels (11 total):**
| Panel | ID | Type |
|:------|:---|:-----|
| Control Panel | `dungeon.control_panel` | Static |
| Room Selector | `dungeon.room_selector` | Static |
| Entrance List | `dungeon.entrance_list` | Static |
| Room Matrix | `dungeon.room_matrix` | Static |
| Entrances Properties | `dungeon.entrances` | Static |
| Room Graphics | `dungeon.room_graphics` | Static |
| Object Editor | `dungeon.object_editor` | Static |
| Debug Controls | `dungeon.debug_controls` | Static |
| Room {id} | `dungeon.room_{id}` | Resource (dynamic) |

**Music Editor Resource Panels:**
| Panel | ID Pattern | Type |
|:------|:-----------|:-----|
| Song Tracker | `music.song_{id}` | Resource (dynamic) |
| Piano Roll | `music.piano_roll_{id}` | Resource (dynamic) |

---

### Remaining Work / Future Enhancements

The core panel system refactor is complete. The following are optional enhancements:

| Enhancement | Priority | Effort | Description |
|:------------|:---------|:-------|:------------|
| Overworld Resource Panels | Medium | 4-6h | Create `overworld.map_{id}` panels for per-map editing |
| Graphics Resource Panels | Medium | 4-6h | Create `graphics.sheet_{id}` panels for per-sheet editing |
| Sprite Resource Panels | Low | 2-4h | Create `sprite.vanilla_{id}` panels |
| Cascade Close | Low | 2-3h | Implement parent-child panel relationships |
| Panel State Persistence | Low | 2-3h | Save/restore pinned state and visibility to config |
| Keyboard Shortcuts | Low | 1-2h | Add configurable shortcuts for common panels |

**Known Limitations:**
- Pin button is in Activity Bar sidebar, not panel title bar (ImGui docking limitation)
- Resource panel limits are enforced but may need tuning based on user feedback
- Some editors (Assembly, Agent) have minimal panel integration

---

### Troubleshooting: Common Panel Visibility Issues

When panels don't respect visibility or appear duplicated, check for these common issues:

#### Issue 1: Duplicate Panel Drawing (DUPLICATE DETECTED warnings)

**Symptom:** Console shows `[PanelWindow] DUPLICATE DETECTED: 'Panel Name' Begin() called twice`

**Cause:** Panel is being drawn by BOTH:
- Central `PanelManager::DrawAllVisiblePanels()` (via EditorPanel)
- Local `gui::PanelWindow` code in the editor's `Update()` method

**Fix:** Remove the local drawing code. When using `RegisterEditorPanel()`, the central drawing handles everything:

```cpp
// WRONG - draws twice
void MyEditor::Initialize() {
  panel_manager->RegisterEditorPanel(std::make_unique<MyPanel>(...));
}
void MyEditor::Update() {
  gui::PanelWindow panel("My Panel", ICON);
  if (panel.Begin(&show_panel_)) { DrawContent(); }
  panel.End();
}

// CORRECT - draws once via central system
void MyEditor::Initialize() {
  panel_manager->RegisterEditorPanel(std::make_unique<MyPanel>([this]() {
    DrawContent();
  }));
}
void MyEditor::Update() {
  // No local drawing - handled by PanelManager::DrawAllVisiblePanels()
  return absl::OkStatus();
}
```

#### Issue 2: Duplicate Registration (RegisterPanel + RegisterEditorPanel)

**Symptom:** Panel appears twice in Activity Bar, or metadata conflicts

**Cause:** Both `RegisterPanel()` AND `RegisterEditorPanel()` called for same panel

**Fix:** Use only `RegisterEditorPanel()` - the EditorPanel class provides all metadata:

```cpp
// WRONG - duplicate registration
panel_manager->RegisterPanel({.card_id = "editor.my_panel", ...});
panel_manager->RegisterEditorPanel(std::make_unique<MyPanel>(...));

// CORRECT - EditorPanel provides metadata via GetId(), GetDisplayName(), etc.
panel_manager->RegisterEditorPanel(std::make_unique<MyPanel>(...));
```

#### Issue 3: Resource Panel Double-Prefixing

**Symptom:** Resource panels (rooms, songs) don't appear or have wrong IDs like `s0.s0.dungeon.room_42`

**Cause:** Using `MakeCardId()` for resource panels when `RegisterPanel()` already adds session prefix

**Fix:** Use base IDs for resource panels - `RegisterPanel()` handles prefixing:

```cpp
// WRONG - double prefix
std::string card_id = MakeCardId(absl::StrFormat("dungeon.room_%d", room_id));
panel_manager->RegisterPanel({.card_id = card_id, ...});

// CORRECT - let RegisterPanel handle prefixing
std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
panel_manager->RegisterPanel({.card_id = card_id, ...});
panel_manager->ShowPanel(card_id);  // Uses same base ID
```

#### Issue 4: Context Menu Popups Don't Open

**Symptom:** Clicking context menu items doesn't open the expected popup (e.g., entity editor)

**Cause:** `ImGui::OpenPopup()` called from within another popup's callback doesn't work

**Fix:** Use deferred popup pattern - store request and process outside popup context:

```cpp
// WRONG - OpenPopup inside context menu callback fails
entity_menu.callback = [this]() {
  InsertEntity();
  ImGui::OpenPopup("Entity Editor");  // Won't work!
};

// CORRECT - defer popup opening
void MyEditor::HandleEntityInsert(const std::string& type) {
  pending_insert_type_ = type;  // Store for later
}

void MyEditor::Update() {
  ProcessPendingInsert();  // Called outside popup context
  // ... draw popups here, OpenPopup() works now
}
```

#### Issue 5: Panels Visible Before Editor Selected

**Symptom:** Panels from other editors appear when on Dashboard

**Cause:** `active_category_` not set to Dashboard, or missing category check

**Fix:** Ensure `SetActiveCategory(kDashboardCategory)` when ROM loaded but no editor selected:

```cpp
void EditorManager::LoadRom() {
  // After loading...
  panel_manager_.SetActiveCategory(PanelManager::kDashboardCategory);
}

void EditorManager::SwitchToEditor(EditorType type) {
  panel_manager_.SetActiveCategory(EditorRegistry::GetEditorCategory(type));
}
```

#### Issue 6: Resource Panels Always Visible (Act Like Pinned)

**Symptom:** Resource panels (rooms, songs) don't hide when switching editors

**Cause:** Missing category filtering in the editor's drawing loop

**Fix:** Add category + pin check before drawing resource panels:

```cpp
// In editor's DrawLayout() for dynamic resource panels:
for (auto& resource : active_resources_) {
  std::string card_id = absl::StrFormat("editor.resource_%d", resource.id);
  
  // Skip if not in category AND not pinned
  if (panel_manager->GetActiveCategory() != "MyEditor" &&
      !panel_manager->IsPanelPinned(card_id)) {
    continue;
  }
  
  // Draw the resource panel...
}
```

### Checklist for Migrating an Editor to EditorPanel System

1. **Create EditorPanel classes** in `panels/` subdirectory
   - Implement `GetId()`, `GetDisplayName()`, `GetIcon()`, `GetEditorCategory()`, `GetPriority()`
   - Implement `Draw(bool* p_open)` with content drawing

2. **Update Initialize()**
   - Call `RegisterEditorPanel()` for each panel
   - Remove any `RegisterPanel()` calls (EditorPanel provides metadata)
   - Call `ShowPanel()` for default-visible panels

3. **Update Update()**
   - Remove ALL local `gui::PanelWindow` drawing code
   - Central `PanelManager::DrawAllVisiblePanels()` handles drawing
   - Keep only non-panel logic (popup modals, shortcuts, etc.)

4. **For Resource Panels** (dynamic, per-resource)
   - Use base IDs (no `MakeCardId()`)
   - Add category filtering before drawing
   - Register on-demand when resource opened
   - Unregister when resource closed

5. **Test**
   - Verify no DUPLICATE DETECTED warnings
   - Verify panels appear/hide on editor switch
   - Verify Activity Bar shows correct panels
   - Verify pinning works across editor switches

---

### Original Migration Plan (For Reference)

### Phase 1: Rename UI Widget (2-3 hours) ✅ COMPLETE

1. Rename `gui::EditorCard` → `gui::PanelWindow`
2. Update all 24 files that use `gui::EditorCard`
3. Update documentation

### Phase 2: Rename Registry (3-4 hours) ✅ COMPLETE

1. Rename `CardInfo` → `PanelDescriptor`
2. Rename `EditorCardRegistry` → `PanelManager`
3. Update all registration calls
4. Update `LayoutPresets` constants

### Phase 3: Create EditorPanel Interface (4-6 hours) ✅ COMPLETE

1. Define `EditorPanel` base class with `PanelCategory`
2. Add pin support to `PanelWindow`
3. Implement central drawing in `PanelManager`

### Phase 4: Migrate Dungeon Editor (Proof of Concept, 8-12 hours) ✅ COMPLETE

1. Convert all 9 Dungeon panels to `EditorPanel` implementations
2. Register with `PanelManager::EmplacePanel<>`
3. Remove manual drawing from `DungeonEditorV2::Update()`
4. Verify LayoutManager docking works

### Phase 5: Migrate Remaining Editors (2-4 hours each) ✅ COMPLETE

Priority order based on panel count:
1. **Palette Editor** (11 panels) ✅ - Has `PaletteGroupPanel` hierarchy
2. **Overworld Editor** (9 panels) ✅ - Most complex, has main canvas
3. **Graphics Editor** (5 panels) ✅ - Mix of legacy and new
4. **Agent Editor** (8 panels) ✅ - Pure panel-based
5. **Music Editor** (7 panels + dynamic) ✅ - Has dynamic song panels
6. **Screen Editor** (5 panels) ✅ - Straightforward
7. **Message Editor** (4 panels) ✅ - Straightforward
8. **Sprite Editor** (2 panels) ✅ - Simple
9. **Assembly Editor** (2 panels) - Deferred (project-based workflow)

---

## Part 8: Summary Statistics & Default Visibility

### 8.1 Panel Counts by Category

| Category | Static Panels | Resource Panels | Default Visible | Notes |
|:---------|:--------------|:----------------|:----------------|:------|
| Overworld | 10 | per-map | 5 | Canvas, Tile16, Area GFX, Usage, Properties |
| Dungeon | 9 | per-room | 5 | Controls, Selector, Matrix, GFX, Object Editor |
| Graphics | 9 | per-sheet | 4 | Browser, Pixel, Palette, 3D Objects |
| Palette | 11 | — | 5 | Controls, OW Main, Dungeon, Sprites, Quick |
| Music | 7 | per-song | 3 | Browser, Tracker, Instruments |
| Screen | 5 | — | 3 | Dungeon Maps, Inventory, Title |
| Sprite | 2 | per-sprite | 1 | Vanilla Editor |
| Message | 4 | — | 3 | List, Editor, Font Atlas |
| Assembly | 2 | per-file | 1 | User-initiated only |
| Agent | 8 | — | 1 | User-initiated only |
| Emulator | 11 | — | 6 | CPU, PPU, Perf, States, Keys, Controller |
| Memory | 1 | — | 1 | User-initiated only |
| **TOTAL** | **~80** | **dynamic** | **35** | ~44% visible by default |

### 8.2 Default Visibility Rationale

**Philosophy**: Show panels that provide immediate value without overwhelming the user.

| Default ON | Reason |
|:-----------|:-------|
| Main canvas/selector | Core editing workflow |
| Graphics preview | Visual feedback while editing |
| Object/entity editors | Primary editing task |
| Usage statistics | Optimization guidance |
| Playback controls | Audio editors need transport |

| Default OFF | Reason |
|:------------|:-------|
| Debug panels | Developer-focused |
| Legacy panels | Replaced by newer versions |
| Advanced tools | Power users enable as needed |
| Agent/AI panels | Requires configuration first |
| Assembly panels | Project-based workflow |

### 8.3 First-Time User Experience

When a ROM is first loaded, show a balanced workspace:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     RECOMMENDED DEFAULT LAYOUT                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────┐  ┌────────────────────────────────────┐  ┌──────────────────┐ │
│  │ Activity │  │                                    │  │ Properties       │ │
│  │ Bar      │  │         Main Canvas                │  │ Panel            │ │
│  │          │  │         (Overworld/Room)           │  │                  │ │
│  │ [OW]     │  │                                    │  │ - Map Info       │ │
│  │ [Dun]    │  │                                    │  │ - Palette        │ │
│  │ [Gfx]    │  │                                    │  │ - GFX Group      │ │
│  │ [Pal]    │  │                                    │  │                  │ │
│  │ [Mus]    │  ├────────────────────────────────────┤  ├──────────────────┤ │
│  │ [Scr]    │  │ Tile Selector    │ Usage Stats    │  │ Graphics         │ │
│  │ [Spr]    │  │                  │                │  │ Preview          │ │
│  │ [Msg]    │  │ [Tile16 Grid]    │ [Heat Map]     │  │                  │ │
│  │ [Asm]    │  │                  │                │  │ [Current Sheet]  │ │
│  │          │  └──────────────────┴────────────────┘  └──────────────────┘ │
│  │ ──────── │                                                               │
│  │ [Emu]    │  Status Bar: ROM Name | Session | Unsaved Changes            │
│  │ [Set]    │                                                               │
│  └──────────┘                                                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Appendix A: File Changes Summary

| File | Changes Required |
|:-----|:-----------------|
| `src/app/gui/app/editor_layout.h` | Rename class, add pin support |
| `src/app/gui/app/editor_layout.cc` | Update implementation |
| `src/app/editor/system/editor_card_registry.h` | Rename to panel_manager.h |
| `src/app/editor/system/editor_card_registry.cc` | Rename, add central drawing |
| `src/app/editor/ui/layout_presets.h` | Rename Card → Panel constants |
| `src/app/editor/menu/activity_bar.cc` | Update to use PanelManager |
| `src/app/editor/*/[editor].cc` (24 files) | Update registrations |

---

## Appendix B: Naming Convention

### Static Panel IDs
- Format: `{category}.{name}` (lowercase, underscores)
- Examples: `dungeon.room_selector`, `palette.ow_main`

### Resource Panel IDs
- Format: `[s{session}.]{category}.{resource}_{id}[.{subpanel}]`
- Session prefix only when `session_count_ > 1`

| Pattern | Example | Description |
|:--------|:--------|:------------|
| `{cat}.{res}_{id}` | `dungeon.room_42` | Single session |
| `s{n}.{cat}.{res}_{id}` | `s0.dungeon.room_42` | Multi-session |
| `s{n}.{cat}.{res}_{id}.{sub}` | `s1.music.song_5.piano_roll` | Subpanel |

### Window Titles
- **Static panels**: `{icon} {Display Name}`
- **Resource panels**: `{icon} {Resource Type} {ID}`
- **Session suffix**: ` [S{n}]` when multi-session

| Panel Type | Single Session | Multi-Session |
|:-----------|:---------------|:--------------|
| Static | `🏰 Room Selector` | `🏰 Room Selector [S0]` |
| Resource | `🚪 Room 42` | `🚪 Room 42 [S1]` |
| Subpanel | `🎹 Song 5 Piano Roll` | `🎹 Song 5 Piano Roll [S0]` |

### Categories (for ActivityBar grouping)
- Match `EditorType`: Overworld, Dungeon, Graphics, Palette, Music, Screen, Sprite, Message, Assembly, Agent, Emulator, Memory

### Resource Types (for resource panels)
| Category | Resource Types |
|:---------|:---------------|
| Dungeon | `room` |
| Music | `song`, `instrument`, `sample` |
| Overworld | `map` |
| Graphics | `sheet` |
| Sprite | `vanilla`, `custom` |
| Message | `text` |
| Assembly | `file` |

---

## Appendix C: ResourcePanel Interface

```cpp
namespace yaze::editor {

/**
 * @class ResourcePanel
 * @brief Base class for panels that edit specific ROM resources
 *
 * A ResourcePanel represents a window for editing a specific piece of
 * data within a ROM, such as a dungeon room, a song, or a graphics sheet.
 *
 * Key Features:
 * - Session-aware: Can distinguish between same resource in different ROMs
 * - Multi-instance: Multiple resources can be open simultaneously
 * - LRU managed: Oldest panels auto-close when limit reached
 *
 * Subclasses:
 * - DungeonRoomPanel: Edits a specific room (0-295)
 * - MusicSongPanel: Edits a specific song with tracker/piano roll
 * - GraphicsSheetPanel: Edits a specific GFX sheet
 * - OverworldMapPanel: Edits a specific overworld map
 */
class ResourcePanel : public EditorPanel {
 public:
  // ========== Resource Identity ==========
  
  /// The numeric ID of the resource (room_id, song_index, sheet_id, etc.)
  virtual int GetResourceId() const = 0;
  
  /// The resource type name (e.g., "room", "song", "sheet")
  virtual std::string GetResourceType() const = 0;
  
  /// Human-readable resource name (e.g., "Hyrule Castle Entrance")
  virtual std::string GetResourceName() const {
    return absl::StrFormat("%s %d", GetResourceType(), GetResourceId());
  }
  
  // ========== Panel Identity (from EditorPanel) ==========
  
  std::string GetId() const override {
    // Generated from resource type and ID
    return absl::StrFormat("%s.%s_%d", 
        GetEditorCategory(), GetResourceType(), GetResourceId());
  }
  
  std::string GetDisplayName() const override {
    return GetResourceName();
  }
  
  // ========== Behavior ==========
  
  /// Resource panels are always editor-bound
  PanelCategory GetPanelCategory() const override { 
    return PanelCategory::EditorBound; 
  }
  
  /// Allow multiple resource panels of same type
  virtual bool AllowMultipleInstances() const { return true; }
  
  /// Get the session ID this resource belongs to
  virtual size_t GetSessionId() const { return session_id_; }
  
  // ========== Lifecycle ==========
  
  /// Called when resource data changes externally
  virtual void OnResourceModified() {}
  
  /// Called when resource is deleted from ROM
  virtual void OnResourceDeleted() { 
    // Default: close the panel
  }
  
 protected:
  size_t session_id_ = 0;
};

/**
 * @brief Example: Dungeon Room Panel
 */
class DungeonRoomPanel : public ResourcePanel {
 public:
  DungeonRoomPanel(size_t session_id, int room_id, 
                   zelda3::Room* room, DungeonCanvasViewer* viewer)
      : room_id_(room_id), room_(room), viewer_(viewer) {
    session_id_ = session_id;
  }
  
  int GetResourceId() const override { return room_id_; }
  std::string GetResourceType() const override { return "room"; }
  std::string GetIcon() const override { return ICON_MD_DOOR_FRONT; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  
  void Draw(bool* p_open) override {
    // Draw room canvas with objects, sprites, etc.
    viewer_->DrawRoom(room_id_, p_open);
  }
  
 private:
  int room_id_;
  zelda3::Room* room_;
  DungeonCanvasViewer* viewer_;
};

}  // namespace yaze::editor
```

---

## Appendix D: Recommended Default Panels by Editor

| Editor | Panels Visible by Default | Rationale |
|:-------|:--------------------------|:----------|
| **Overworld** | Canvas, Tile16, Area GFX, Usage Stats, Properties | Full tile editing workflow |
| **Dungeon** | Controls, Selector, Matrix, Room GFX, Object Editor | Room editing + visual navigation |
| **Graphics** | Sheet Browser, Pixel Editor, Palette Controls, 3D Objects | Complete pixel art workflow |
| **Palette** | Controls, OW Main, Dungeon Main, Sprites, Quick Access | Color editing across contexts |
| **Music** | Song Browser, Tracker, Instrument Editor | Music composition workflow |
| **Screen** | Dungeon Maps, Inventory, Title Screen | Most commonly edited screens |
| **Sprite** | Vanilla Editor | View-only by default, editing opt-in |
| **Message** | Message List, Message Editor, Font Atlas | Text editing workflow |
| **Assembly** | *None* | Project-based, user-initiated |
| **Agent** | *None* | Requires API config first |
| **Emulator** | CPU Debugger, PPU, Performance, Save States, Keys, Controller | Debugging + playback |
| **Memory** | *None* | Power user feature |
