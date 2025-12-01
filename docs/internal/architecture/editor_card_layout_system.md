# Editor Panel (Card) and Layout System Architecture

> Migration note: Phase 2 renames Card → Panel (`PanelWindow`, `PanelManager`,
> `PanelDescriptor`). The concepts below still use legacy Card naming; apply the
> new Panel terms when implementing changes.

This document describes the yaze editor's card-based UI system, layout management, and how they integrate with the agent system.

## Overview

The yaze editor uses a modular card-based architecture inspired by VSCode's workspace model:
- **Cards** = Dockable ImGui windows representing editor components
- **Categories** = Logical groupings (Dungeon, Overworld, Graphics, etc.)
- **Layouts** = DockBuilder configurations defining window arrangements
- **Presets** = Named visibility configurations for quick switching

## System Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              EditorManager                                   │
│  (Central coordinator - owns all components below)                          │
├──────────────────────┬───────────────────────┬──────────────────────────────┤
│   EditorCardRegistry │     LayoutManager      │      UICoordinator          │
│   ─────────────────  │     ─────────────      │      ─────────────          │
│   • Card metadata    │     • DockBuilder      │      • UI state flags       │
│   • Visibility mgmt  │     • Default layouts  │      • Menu drawing         │
│   • Session prefixes │     • Window arrange   │      • Popup coordination   │
│   • Workspace presets│     • Per-editor setup │      • Command palette      │
├──────────────────────┴───────────────────────┴──────────────────────────────┤
│                              LayoutPresets                                   │
│  (Static definitions - default cards per editor type)                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Component Relationships

```
                    ┌──────────────────┐
                    │   EditorManager  │
                    │  (coordinator)   │
                    └────────┬─────────┘
                             │ owns
          ┌──────────────────┼──────────────────┐
          │                  │                  │
          ▼                  ▼                  ▼
┌─────────────────┐  ┌──────────────┐  ┌───────────────┐
│EditorCardRegistry│  │LayoutManager │  │ UICoordinator │
└────────┬────────┘  └───────┬──────┘  └───────┬───────┘
         │                   │                  │
         │ queries           │ uses             │ delegates
         ▼                   ▼                  ▼
┌─────────────────┐  ┌──────────────┐  ┌───────────────┐
│  LayoutPresets  │  │ EditorCard   │  │ EditorCard    │
│  (card defaults)│  │ Registry     │  │ Registry      │
└─────────────────┘  │ (window      │  │ (emulator     │
                     │  titles)     │  │  visibility)  │
                     └──────────────┘  └───────────────┘
```

---

## EditorCardRegistry

**File:** `src/app/editor/system/editor_card_registry.h`

### CardInfo Structure

Every card is registered with complete metadata:

```cpp
struct CardInfo {
  std::string card_id;           // "dungeon.room_selector"
  std::string display_name;      // "Room Selector"
  std::string window_title;      // " Rooms List" (matches ImGui::Begin)
  std::string icon;              // ICON_MD_LIST
  std::string category;          // "Dungeon"
  std::string shortcut_hint;     // "Ctrl+Shift+R"
  bool* visibility_flag;         // &show_room_selector_
  EditorCard* card_instance;     // Optional card pointer
  std::function<void()> on_show; // Callback when shown
  std::function<void()> on_hide; // Callback when hidden
  int priority;                  // Menu ordering (lower = higher)

  // Disabled state support
  std::function<bool()> enabled_condition;  // ROM-dependent cards
  std::string disabled_tooltip;             // "Load a ROM first"
};
```

### Card Categories

| Category    | Icon                      | Purpose                      |
|-------------|---------------------------|------------------------------|
| Dungeon     | `ICON_MD_CASTLE`          | Dungeon room editing         |
| Overworld   | `ICON_MD_MAP`             | Overworld map editing        |
| Graphics    | `ICON_MD_IMAGE`           | Graphics/tile sheet editing  |
| Palette     | `ICON_MD_PALETTE`         | Palette editing              |
| Sprite      | `ICON_MD_PERSON`          | Sprite management            |
| Music       | `ICON_MD_MUSIC_NOTE`      | Audio/music editing          |
| Message     | `ICON_MD_MESSAGE`         | Text/message editing         |
| Screen      | `ICON_MD_TV`              | Screen/UI editing            |
| Emulator    | `ICON_MD_VIDEOGAME_ASSET` | Emulation & debugging        |
| Assembly    | `ICON_MD_CODE`            | ASM code editing             |
| Settings    | `ICON_MD_SETTINGS`        | Application settings         |
| Memory      | `ICON_MD_MEMORY`          | Memory inspection            |
| Agent       | `ICON_MD_SMART_TOY`       | AI agent controls            |

### Session-Aware Card IDs

Cards support multi-session (multiple ROMs open):

```
Single session:   "dungeon.room_selector"
Multiple sessions: "s0.dungeon.room_selector", "s1.dungeon.room_selector"
```

The registry automatically prefixes card IDs using `MakeCardId()` and `GetPrefixedCardId()`.

### VSCode-Style Sidebar Layout

```
┌────┬─────────────────────────────────┬────────────────────────────────────────────┐
│ AB │          Side Panel             │           Main Docking Space              │
│    │         (250px width)           │                                           │
│    ├─────────────────────────────────┤                                           │
│ 48 │ ▶ Dungeon                       │  ┌────────────────────────────────────┐   │
│ px │   ☑ Control Panel               │  │                                    │   │
│    │   ☑ Room Selector               │  │     Docked Editor Windows          │   │
│ w  │   ☐ Object Editor               │  │                                    │   │
│ i  │   ☐ Room Matrix                 │  │                                    │   │
│ d  │                                 │  │                                    │   │
│ e  │ ▶ Graphics                      │  │                                    │   │
│    │   ☐ Sheet Browser               │  │                                    │   │
│    │   ☐ Tile Editor                 │  │                                    │   │
│    │                                 │  └────────────────────────────────────┘   │
│    │ ▶ Palette                       │                                           │
│    │   ☐ Control Panel               │                                           │
├────┴─────────────────────────────────┴───────────────────────────────────────────┤
│                                 Status Bar                                        │
└──────────────────────────────────────────────────────────────────────────────────┘
  ↑
  Activity Bar (category icons)
```

### Unified Visibility Management

The registry is the **single source of truth** for component visibility:

```cpp
// Emulator visibility (delegated from UICoordinator)
bool IsEmulatorVisible() const;
void SetEmulatorVisible(bool visible);
void ToggleEmulatorVisible();
void SetEmulatorVisibilityChangedCallback(std::function<void(bool)> cb);
```

### Card Validation System

Catches window title mismatches during development:

```cpp
struct CardValidationResult {
  std::string card_id;
  std::string expected_title;  // From CardInfo::GetWindowTitle()
  bool found_in_imgui;         // Whether ImGui found window
  std::string message;         // Human-readable status
};

std::vector<CardValidationResult> ValidateCards() const;
void DrawValidationReport(bool* p_open);
```

---

## LayoutPresets

**File:** `src/app/editor/ui/layout_presets.h`

### Default Layouts Per Editor

Each editor type has a defined set of default and optional cards:

```cpp
struct PanelLayoutPreset {
  std::string name;                           // "Overworld Default"
  std::string description;                    // Human-readable
  EditorType editor_type;                     // EditorType::kOverworld
  std::vector<std::string> default_visible_cards;  // Shown on first open
  std::vector<std::string> optional_cards;         // Available but hidden
};
```

### Editor Default Cards

| Editor     | Default Cards                              | Optional Cards                        |
|------------|-------------------------------------------|---------------------------------------|
| Overworld  | Canvas, Tile16 Selector                   | Tile8, Area GFX, Scratch, Usage Stats |
| Dungeon    | Control Panel, Room Selector              | Object Editor, Palette, Room Matrix   |
| Graphics   | Sheet Browser, Sheet Editor               | Player Animations, Prototype Viewer   |
| Palette    | Control Panel, OW Main                    | Quick Access, OW Animated, Dungeon    |
| Sprite     | Vanilla Editor                            | Custom Editor                         |
| Screen     | Dungeon Maps                              | Title, Inventory, OW Map, Naming      |
| Music      | Tracker                                   | Instrument Editor, Assembly           |
| Message    | Message List, Message Editor              | Font Atlas, Dictionary                |
| Assembly   | Editor                                    | File Browser                          |
| Emulator   | PPU Viewer                                | CPU Debugger, Memory, Breakpoints     |
| Agent      | Configuration, Status, Chat               | Prompt Editor, Profiles, History      |

### Named Workspace Presets

| Preset Name       | Focus                | Key Cards                                  |
|-------------------|----------------------|-------------------------------------------|
| Minimal           | Essential editing    | Main canvas only                          |
| Developer         | Debug/development    | Emulator, Assembly, Memory, CPU Debugger  |
| Designer          | Visual/artistic      | Graphics, Palette, Sprites, Screens       |
| Modder            | Full-featured        | Everything enabled                        |
| Overworld Expert  | Complete OW toolkit  | All OW cards + Palette + Graphics         |
| Dungeon Expert    | Complete dungeon     | All dungeon cards + Palette + Graphics    |
| Testing           | QA focused           | Emulator, Save States, CPU, Memory, Agent |
| Audio             | Music focused        | Tracker, Instruments, Assembly, APU       |

---

## LayoutManager

**File:** `src/app/editor/ui/layout_manager.h`

Manages ImGui DockBuilder layouts for each editor type.

### Default Layout Patterns

**Overworld Editor:**
```
┌─────────────────────────────┬──────────────┐
│                             │              │
│  Overworld Canvas (75%)     │ Tile16 (25%) │
│  (Main editing area)        │  Selector    │
│                             │              │
└─────────────────────────────┴──────────────┘
```

**Dungeon Editor:**
```
┌─────┬───────────────────────────────────┐
│     │                                   │
│Room │  Dungeon Controls (85%)           │
│(15%)│  (Main editing area, maximized)   │
│     │                                   │
└─────┴───────────────────────────────────┘
```

**Graphics Editor:**
```
┌──────────────┬──────────────────────────┐
│              │                          │
│ Sheet        │ Sheet Editor (75%)       │
│ Browser      │ (Main canvas with tabs)  │
│ (25%)        │                          │
└──────────────┴──────────────────────────┘
```

**Message Editor:**
```
┌─────────────┬──────────────────┬──────────┐
│ Message     │ Message          │ Font     │
│ List (25%)  │ Editor (50%)     │ Atlas    │
│             │                  │ (25%)    │
│             ├──────────────────┤          │
│             │                  │Dictionary│
└─────────────┴──────────────────┴──────────┘
```

---

## Agent UI System

The agent UI system provides AI-assisted editing with a multi-agent architecture.

### Component Hierarchy

```
┌────────────────────────────────────────────────────────────────────────┐
│                         AgentUiController                              │
│  (Central coordinator for all agent UI components)                     │
├─────────────────────┬─────────────────────┬────────────────────────────┤
│  AgentSessionManager│    AgentSidebar     │   AgentChatCard[]          │
│  ──────────────────│    ─────────────     │   ───────────────          │
│  • Session lifecycle│    • Tab bar        │   • Dockable windows       │
│  • Active session   │    • Model selector │   • Full chat view         │
│  • Card open state  │    • Chat compact   │   • Per-agent instance     │
│                     │    • Proposals panel│                            │
├─────────────────────┼─────────────────────┼────────────────────────────┤
│  AgentEditor        │    AgentChatView    │   AgentProposalsPanel      │
│  ─────────────      │    ──────────────   │   ───────────────────      │
│  • Configuration    │    • Message list   │   • Code proposals         │
│  • Profile mgmt     │    • Input box      │   • Accept/reject          │
│  • Status display   │    • Send button    │   • Apply changes          │
└─────────────────────┴─────────────────────┴────────────────────────────┘
```

### AgentSession Structure

```cpp
struct AgentSession {
  std::string agent_id;          // Unique identifier (UUID)
  std::string display_name;      // "Agent 1", "Agent 2"
  AgentUIContext context;        // Shared state with all views
  bool is_active = false;        // Currently selected in tab bar
  bool has_card_open = false;    // Pop-out card is visible

  // Callbacks shared between sidebar and pop-out cards
  ChatCallbacks chat_callbacks;
  ProposalCallbacks proposal_callbacks;
  CollaborationCallbacks collaboration_callbacks;
};
```

### AgentSessionManager

Manages multiple concurrent agent sessions:

```cpp
class AgentSessionManager {
public:
  std::string CreateSession(const std::string& name = "");
  void CloseSession(const std::string& agent_id);

  AgentSession* GetActiveSession();
  AgentSession* GetSession(const std::string& agent_id);
  void SetActiveSession(const std::string& agent_id);

  void OpenCardForSession(const std::string& agent_id);
  void CloseCardForSession(const std::string& agent_id);

  size_t GetSessionCount() const;
  std::vector<AgentSession>& GetAllSessions();
};
```

### Sidebar Layout

```
┌─────────────────────────────────────────┐
│ [Agent 1] [Agent 2] [+]                 │ ← Tab bar with new agent button
├─────────────────────────────────────────┤
│ Model: gemini-2 ▼  👤  [↗ Pop-out]      │ ← Header with model selector
├─────────────────────────────────────────┤
│                                         │
│   💬 User: How do I edit tiles?        │
│                                         │
│   🤖 Agent: You can use the Tile16...  │ ← Chat messages (scrollable)
│                                         │
├─────────────────────────────────────────┤
│ [Type a message...]              [Send] │ ← Input box
├─────────────────────────────────────────┤
│ ▶ Proposals (3)                         │ ← Collapsible section
│   • prop-001 ✓ Applied                  │
│   • prop-002 ⏳ Pending                 │
│   • prop-003 ❌ Rejected                │
└─────────────────────────────────────────┘
```

### Pop-Out Card Flow

```
User clicks [↗ Pop-out] button in sidebar
        │
        ▼
AgentSidebar::pop_out_callback_()
        │
        ▼
AgentUiController::PopOutAgent(agent_id)
        │
        ├─► Create AgentChatCard(agent_id, &session_manager_)
        │
        ├─► card->SetToastManager(toast_manager_)
        │
        ├─► card->SetAgentService(agent_service)
        │
        ├─► session_manager_.OpenCardForSession(agent_id)
        │
        └─► open_cards_.push_back(std::move(card))

Each frame in Update():
        │
        ▼
AgentUiController::DrawOpenCards()
        │
        ├─► For each card in open_cards_:
        │       bool open = true;
        │       card->Draw(&open);
        │       if (!open) {
        │           session_manager_.CloseCardForSession(card->agent_id());
        │           Remove from open_cards_
        │       }
        │
        └─► Pop-out cards render in main docking space
```

### State Synchronization

Both sidebar and pop-out cards share the same `AgentSession::context`:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         AgentUIContext                                   │
│  (Shared state for all views of same agent session)                     │
├─────────────────────────────────────────────────────────────────────────┤
│  agent_config_     │ Provider, model, API keys, flags                   │
│  chat_messages_    │ Conversation history                               │
│  pending_proposals_│ Code changes awaiting approval                     │
│  collaboration_    │ Multi-user collaboration state                     │
│  rom_              │ Reference to loaded ROM                            │
│  changed_          │ Flag for detecting config changes                  │
└─────────────────────────────────────────────────────────────────────────┘
                              ↑
              ┌───────────────┼───────────────┐
              │               │               │
    ┌─────────┴───────┐  ┌────┴────┐  ┌──────┴──────┐
    │  AgentSidebar   │  │AgentChat │  │AgentChat   │
    │  (compact view) │  │ Card 1   │  │ Card 2     │
    │  (right panel)  │  │ (docked) │  │ (floating) │
    └─────────────────┘  └──────────┘  └────────────┘
```

---

## Card Registration Pattern

Cards are registered during editor initialization:

```cpp
void DungeonEditor::Initialize(EditorDependencies& deps) {
  deps.card_registry->RegisterCard({
      .card_id = MakeCardId("dungeon.room_selector"),
      .display_name = "Room Selector",
      .window_title = " Rooms List",     // Must match ImGui::Begin()
      .icon = ICON_MD_LIST,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+R",
      .visibility_flag = &show_room_selector_,
      .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
      .disabled_tooltip = "Load a ROM to access room selection",
      .priority = 10
  });
}
```

### Registration Best Practices

1. **Use `MakeCardId()`** - Applies session prefixing if needed
2. **Match window_title exactly** - Must match ImGui::Begin() call
3. **Use Material Design icons** - `ICON_MD_*` constants
4. **Set category correctly** - Groups in sidebar
5. **Provide visibility_flag** - Points to bool member variable
6. **Include enabled_condition** - For ROM-dependent cards
7. **Set priority** - Lower = higher in menus

---

## Initialization Flow

```
Application Startup
        │
        ▼
EditorManager::Initialize()
        │
        ├─► Create EditorCardRegistry
        │
        ├─► Create LayoutManager (linked to registry)
        │
        ├─► Create UICoordinator (with registry reference)
        │
        └─► For each Editor:
                │
                └─► Editor::Initialize(deps)
                        │
                        └─► deps.card_registry->RegisterCard(...)
                              (registers all cards for this editor)
```

## Editor Switch Flow

```
User clicks editor in menu
        │
        ▼
EditorManager::SwitchToEditor(EditorType type)
        │
        ├─► HideCurrentEditorCards()
        │       └─► card_registry_.HideAllCardsInCategory(old_category)
        │
        ├─► LayoutManager::InitializeEditorLayout(type, dockspace_id)
        │       └─► Build{EditorType}Layout() using DockBuilder
        │
        ├─► LayoutPresets::GetDefaultCards(type)
        │       └─► Returns default_visible_cards for this editor
        │
        ├─► For each default card:
        │       card_registry_.ShowCard(session_id, card_id)
        │
        └─► current_editor_ = editors_[type]
```

---

## File Locations

| Component            | Header                                           | Implementation                                    |
|----------------------|--------------------------------------------------|--------------------------------------------------|
| EditorCardRegistry   | `src/app/editor/system/editor_card_registry.h`   | `src/app/editor/system/editor_card_registry.cc`  |
| LayoutManager        | `src/app/editor/ui/layout_manager.h`             | `src/app/editor/ui/layout_manager.cc`            |
| LayoutPresets        | `src/app/editor/ui/layout_presets.h`             | `src/app/editor/ui/layout_presets.cc`            |
| UICoordinator        | `src/app/editor/ui/ui_coordinator.h`             | `src/app/editor/ui/ui_coordinator.cc`            |
| EditorManager        | `src/app/editor/editor_manager.h`                | `src/app/editor/editor_manager.cc`               |
| AgentUiController    | `src/app/editor/agent/agent_ui_controller.h`     | `src/app/editor/agent/agent_ui_controller.cc`    |
| AgentSessionManager  | `src/app/editor/agent/agent_session.h`           | `src/app/editor/agent/agent_session.cc`          |
| AgentSidebar         | `src/app/editor/agent/agent_sidebar.h`           | `src/app/editor/agent/agent_sidebar.cc`          |
| AgentChatCard        | `src/app/editor/agent/agent_chat_card.h`         | `src/app/editor/agent/agent_chat_card.cc`        |
| AgentChatView        | `src/app/editor/agent/agent_chat_view.h`         | `src/app/editor/agent/agent_chat_view.cc`        |
| AgentProposalsPanel  | `src/app/editor/agent/agent_proposals_panel.h`   | `src/app/editor/agent/agent_proposals_panel.cc`  |
| AgentState           | `src/app/editor/agent/agent_state.h`             | (header-only)                                    |

---

## See Also

- [Graphics System Architecture](graphics_system_architecture.md)
- [Dungeon Editor System](dungeon_editor_system.md)
- [Overworld Editor System](overworld_editor_system.md)
