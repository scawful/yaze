#include "dungeon_editor_v2.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"

#include "app/editor/dungeon/panels/dungeon_entrance_list_panel.h"
#include "app/editor/dungeon/panels/dungeon_entrances_panel.h"
#include "app/editor/dungeon/panels/dungeon_object_editor_panel.h"
#include "app/editor/dungeon/panels/dungeon_palette_editor_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_graphics_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_matrix_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_selector_panel.h"
#include "app/editor/system/panel_manager.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

// No table layout needed - all cards are independent

void DungeonEditorV2::Initialize(gfx::IRenderer* renderer, Rom* rom) {
  renderer_ = renderer;
  rom_ = rom;

  // Propagate ROM to all rooms
  if (rom_) {
    for (auto& room : rooms_) {
      room.SetRom(rom_);
    }
  }

  // Don't initialize emulator preview yet - ROM might not be loaded
  // Will be initialized in Load() instead

  // Setup docking class for room windows (ImGui::GetID will be called in Update
  // when ImGui is ready)
  room_window_class_.DockingAllowUnclassed =
      true;  // Room windows can dock with anything
  room_window_class_.DockingAlwaysTabBar =
      true;  // Always show tabs when multiple rooms

  // Show control panel and room selector by default when Dungeon Editor is
  // activated. Set these BEFORE card registration so cards appear even if
  // registry is unavailable.
  show_control_panel_ = true;
  show_room_selector_ = true;

  // Register all cards with PanelManager (dependency injection)
  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.control_panel"),
                               .display_name = "Dungeon Controls",
                               .window_title = " Dungeon Controls",
                               .icon = ICON_MD_CASTLE,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+D",
                               .visibility_flag = &show_control_panel_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to access dungeon controls",
                               .priority = 10});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.room_selector"),
                               .display_name = "Room List",
                               .window_title = " Room List",
                               .icon = ICON_MD_LIST,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+R",
                               .visibility_flag = &show_room_selector_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to browse dungeon rooms",
                               .priority = 20});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.entrance_list"),
                               .display_name = "Entrance List",
                               .window_title = " Entrance List",
                               .icon = ICON_MD_DOOR_FRONT,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+E",
                               .visibility_flag = &show_entrances_list_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to browse dungeon entrances",
                               .priority = 25});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.entrance_properties"),
                               .display_name = "Entrance Properties",
                               .window_title = " Entrance Properties",
                               .icon = ICON_MD_TUNE,
                               .category = "Dungeon",
                               .shortcut_hint = "",
                               .visibility_flag = nullptr,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to edit entrance properties",
                               .priority = 26});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.room_matrix"),
                               .display_name = "Room Matrix",
                               .window_title = " Room Matrix",
                               .icon = ICON_MD_GRID_VIEW,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+M",
                               .visibility_flag = &show_room_matrix_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to view the room matrix",
                               .priority = 30});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.room_graphics"),
                               .display_name = "Room Graphics",
                               .window_title = " Room Graphics",
                               .icon = ICON_MD_IMAGE,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+G",
                               .visibility_flag = &show_room_graphics_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to view room graphics",
                               .priority = 50});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.object_tools"),
                               .display_name = "Object Tools",
                               .window_title = " Object Tools",
                               .icon = ICON_MD_CONSTRUCTION,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+O",
                               .visibility_flag = &show_object_editor_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to edit dungeon objects",
                               .priority = 60});

  panel_manager->RegisterPanel({.card_id = MakeCardId("dungeon.palette_editor"),
                               .display_name = "Palette Editor",
                               .window_title = " Palette Editor",
                               .icon = ICON_MD_PALETTE,
                               .category = "Dungeon",
                               .shortcut_hint = "Ctrl+Shift+P",
                               .visibility_flag = &show_palette_editor_,
                               .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM to edit dungeon palettes",
                               .priority = 70});





  // ============================================================================
  // Phase 5: Full EditorPanel Registration
  // Register EditorPanel instances for central drawing via DrawAllVisiblePanels()
  // These panels wrap existing components with the EditorPanel interface.
  // ============================================================================

  // Room Selector Panel - room list with filter
  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonRoomSelectorPanel>(
          &room_selector_,
          [this](int room_id) { OnRoomSelected(room_id); }));

  // Entrance List Panel - entrance list with filter (separate from properties)
  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonEntranceListPanel>(
          &room_selector_,
          [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  // Room Matrix Panel - 16x19 visual grid for room navigation
  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonRoomMatrixPanel>(
          &current_room_id_,
          &active_rooms_,
          [this](int room_id) { OnRoomSelected(room_id); }));

  // Entrances Properties Panel - entrance properties editor
  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonEntrancesPanel>(
          &entrances_,
          &current_entrance_id_,
          [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  // Room Graphics Panel - displays room graphics blocks
  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonRoomGraphicsPanel>(
          &current_room_id_,
          &rooms_));

  // Palette Editor Panel - wraps PaletteEditorWidget
  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonPaletteEditorPanel>(&palette_editor_));





  // NOTE: DungeonObjectEditorPanel is registered in Load() after object_editor_card_ is created
}

void DungeonEditorV2::Initialize() {}

absl::Status DungeonEditorV2::Load() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Load all rooms using the loader component - DEFERRED for lazy loading
  // RETURN_IF_ERROR(room_loader_.LoadAllRooms(rooms_));
  RETURN_IF_ERROR(room_loader_.LoadRoomEntrances(entrances_));

  // Load palette group
  auto dungeon_main_pal_group = rom_->palette_group().dungeon_main;
  current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
  ASSIGN_OR_RETURN(current_palette_group_,
                   gfx::CreatePaletteGroupFromLargePalette(current_palette_));

  // Initialize components with loaded data
  room_selector_.set_rooms(&rooms_);
  room_selector_.set_entrances(&entrances_);
  room_selector_.set_active_rooms(active_rooms_);
  room_selector_.set_room_selected_callback(
      [this](int room_id) { OnRoomSelected(room_id); });

  canvas_viewer_.SetRooms(&rooms_);
  canvas_viewer_.SetRenderer(renderer_);
  canvas_viewer_.SetCurrentPaletteGroup(current_palette_group_);
  canvas_viewer_.SetCurrentPaletteId(current_palette_id_);

  // Capture mutations for undo/redo snapshots
  canvas_viewer_.object_interaction().SetMutationHook(
      [this]() { PushUndoSnapshot(current_room_id_); });

  // Wire up cache invalidation callback for object interaction
  canvas_viewer_.object_interaction().SetCacheInvalidationCallback([this]() {
    // Trigger room re-render after object changes
    if (current_room_id_ >= 0 &&
        current_room_id_ < static_cast<int>(rooms_.size())) {
      rooms_[current_room_id_].RenderRoomGraphics();
    }
  });

  // Wire up object placed callback for canvas interaction
  canvas_viewer_.object_interaction().SetObjectPlacedCallback(
      [this](const zelda3::RoomObject& obj) { HandleObjectPlaced(obj); });

  // Create render service if not already created (set_rom() may have created it)
  if (!render_service_) {
    render_service_ = std::make_unique<emu::render::EmulatorRenderService>(rom_);
    auto status = render_service_->Initialize();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to initialize render service: %s",
                status.message().data());
      // Non-fatal - preview will fall back to legacy mode
    }
  }



  // Initialize centralized PaletteManager with ROM data
  // This MUST be done before initializing palette_editor_
  gfx::PaletteManager::Get().Initialize(rom_);

  // Initialize palette editor with loaded ROM
  palette_editor_.Initialize(rom_);

  // Initialize DungeonEditorSystem (currently scaffolding for persistence and metadata)
  dungeon_editor_system_ = std::make_unique<zelda3::DungeonEditorSystem>(rom_);
  (void)dungeon_editor_system_->Initialize();
  dungeon_editor_system_->SetCurrentRoom(current_room_id_);

  // Initialize unified object editor card
  object_editor_card_ = std::make_unique<ObjectEditorCard>(
      renderer_, rom_, &canvas_viewer_, dungeon_editor_system_->GetObjectEditor());

  // Register ObjectEditorPanel (deferred from Initialize() because object_editor_card_ is created here)
  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->RegisterEditorPanel(
        std::make_unique<DungeonObjectToolsPanel>(object_editor_card_.get()));
  }

  // Link editor system to canvas viewer for interactions
  if (dungeon_editor_system_) {
    canvas_viewer_.SetEditorSystem(dungeon_editor_system_.get());
  }

  // Wire palette changes to trigger room re-renders
  // PaletteManager now tracks all modifications globally
  palette_editor_.SetOnPaletteChanged([this](int /*palette_id*/) {
    // Re-render all active rooms when palette changes
    for (int i = 0; i < active_rooms_.Size; i++) {
      int room_id = active_rooms_[i];
      if (room_id >= 0 && room_id < (int)rooms_.size()) {
        rooms_[room_id].RenderRoomGraphics();
      }
    }
  });

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Update() {
  const auto& theme = AgentUI::GetTheme();
  // Initialize docking class ID on first Update (when ImGui is ready)
  if (room_window_class_.ClassId == 0) {
    room_window_class_.ClassId = ImGui::GetID("DungeonRoomClass");
  }

  if (!is_loaded_) {
    // CARD-BASED EDITOR: Create a minimal loading card
    gui::PanelWindow loading_card("Dungeon Editor Loading", ICON_MD_CASTLE);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::TextColored(theme.text_secondary_gray,
                         "Loading dungeon data...");
      ImGui::TextWrapped(
          "Independent editor cards will appear once ROM data is loaded.");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  // CARD-BASED EDITOR: All windows are independent top-level cards
  // No parent wrapper - this allows closing control panel without affecting
  // rooms

  // Note: Central drawing via EditorPanel instances is now handled by
  // EditorManager::Update() calling PanelManager::DrawAllVisiblePanels()
  // once per frame for all editors.

  DrawLayout();

  // Handle keyboard shortcuts for object manipulation
  // Delete key - remove selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    canvas_viewer_.DeleteSelectedObjects();
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Undo() {
  if (dungeon_editor_system_) {
    return dungeon_editor_system_->Undo();
  }
  return absl::UnimplementedError("Undo not available");
}

absl::Status DungeonEditorV2::Redo() {
  if (dungeon_editor_system_) {
    return dungeon_editor_system_->Redo();
  }
  return absl::UnimplementedError("Redo not available");
}

absl::Status DungeonEditorV2::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Save palette changes first (if any)
  if (gfx::PaletteManager::Get().HasUnsavedChanges()) {
    auto status = gfx::PaletteManager::Get().SaveAllToRom();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save palette changes: %s",
                status.message().data());
      return status;
    }
    LOG_INFO("DungeonEditorV2", "Saved %zu modified colors to ROM",
             gfx::PaletteManager::Get().GetModifiedColorCount());
  }

  // Save all rooms (SaveObjects will handle which ones need saving)
  for (auto& room : rooms_) {
    auto status = room.SaveObjects();
    if (!status.ok()) {
      // Log error but continue with other rooms
      LOG_ERROR("DungeonEditorV2", "Failed to save room objects: %s",
                status.message().data());
    }

    // Save sprites and other entities via system
    if (dungeon_editor_system_) {
      auto sys_status = dungeon_editor_system_->SaveRoom(room.id());
      if (!sys_status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Failed to save room system data: %s",
                  sys_status.message().data());
      }
    }
  }

  // Save additional dungeon state (stubbed) via DungeonEditorSystem when present
  if (dungeon_editor_system_) {
    auto status = dungeon_editor_system_->SaveDungeon();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "DungeonEditorSystem save failed: %s",
                status.message().data());
      return status;
    }
  }

  return absl::OkStatus();
}

void DungeonEditorV2::DrawLayout() {
  // ============================================================================
  // Phase 4 Complete: Static panels now drawn by DrawAllVisiblePanels()
  // Only dynamic room cards remain here (will migrate to ResourcePanel in Phase 6)
  // ============================================================================

  // Dynamic Room Cards - each open room gets its own dockable card
  // Synced with PanelManager for Activity Bar visibility and category filtering
  for (int i = 0; i < active_rooms_.Size; i++) {
    int room_id = active_rooms_[i];
    // Use base ID - PanelManager handles session prefixing
    std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
    
    // Check if panel was hidden via Activity Bar
    bool panel_visible = true;
    if (dependencies_.panel_manager) {
      panel_visible = dependencies_.panel_manager->IsPanelVisible(card_id);
    }
    
    // If hidden via Activity Bar, close the room
    if (!panel_visible) {
      dependencies_.panel_manager->UnregisterPanel(card_id);
      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      i--;
      continue;
    }
    
    // Category filtering: only draw if Dungeon is active OR panel is pinned
    bool is_pinned = dependencies_.panel_manager && 
                     dependencies_.panel_manager->IsPanelPinned(card_id);
    std::string active_category = dependencies_.panel_manager ? 
                                  dependencies_.panel_manager->GetActiveCategory() : "";
    
    if (active_category != "Dungeon" && !is_pinned) {
      // Not in Dungeon editor and not pinned - skip drawing but keep registered
      // Panel will reappear when user returns to Dungeon editor
      continue;
    }
    
    bool open = true;

    // Create session-aware card title with room ID prominent
    std::string base_name;
    if (room_id >= 0 &&
        static_cast<size_t>(room_id) < std::size(zelda3::kRoomNames)) {
      base_name = absl::StrFormat("[%03X] %s", room_id,
                                  zelda3::kRoomNames[room_id].data());
    } else {
      base_name = absl::StrFormat("Room %03X", room_id);
    }

    std::string card_name_str = absl::StrFormat(
        "%s###RoomCard%d", MakeCardTitle(base_name).c_str(), room_id);

    // Track or create card for jump-to functionality
    if (room_cards_.find(room_id) == room_cards_.end()) {
      room_cards_[room_id] = std::make_shared<gui::PanelWindow>(
          card_name_str.c_str(), ICON_MD_GRID_ON, &open);
      room_cards_[room_id]->SetDefaultSize(700, 600);

      // Set default position for first room to be docked with main window
      if (active_rooms_.Size == 1) {
        room_cards_[room_id]->SetPosition(gui::PanelWindow::Position::Floating);
      }
    }

    auto& room_card = room_cards_[room_id];

    // CRITICAL: Use docking class BEFORE Begin() to make rooms dock together
    // This creates a separate docking space for all room cards
    ImGui::SetNextWindowClass(&room_window_class_);

    // Make room cards fully dockable and independent
    if (room_card->Begin(&open)) {
      DrawRoomTab(room_id);
    }
    room_card->End();

    if (!open) {
      // Unregister from PanelManager so it's removed from Activity Bar
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }
      
      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      i--;
    }
  }
}

void DungeonEditorV2::DrawRoomTab(int room_id) {
  const auto& theme = AgentUI::GetTheme();
  if (room_id < 0 || room_id >= 0x128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  auto& room = rooms_[room_id];

  // Lazy load room data
  if (!room.IsLoaded()) {
    auto status = room_loader_.LoadRoom(room_id, room);
    if (!status.ok()) {
      ImGui::TextColored(theme.text_error_red, "Failed to load room: %s",
                         status.message().data());
      return;
    }
    
    // Load system data for this room (sprites, etc.)
    if (dungeon_editor_system_) {
      auto sys_status = dungeon_editor_system_->ReloadRoom(room_id);
      if (!sys_status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Failed to load system data: %s",
                  sys_status.message().data());
      }
    }
  }

  // Initialize room graphics and objects in CORRECT ORDER
  // Critical sequence: 1. Load data from ROM, 2. Load objects (sets floor
  // graphics), 3. Render
  if (room.IsLoaded()) {
    bool needs_render = false;

    // Step 1: Load room data from ROM (blocks, blockset info)
    if (room.blocks().empty()) {
      room.LoadRoomGraphics(room.blockset);
      needs_render = true;
      LOG_DEBUG("[DungeonEditorV2]", "Loaded room %d graphics from ROM",
                room_id);
    }

    // Step 2: Load objects from ROM (CRITICAL: sets floor1_graphics_,
    // floor2_graphics_!)
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
      needs_render = true;
      LOG_DEBUG("[DungeonEditorV2]", "Loaded room %d objects from ROM",
                room_id);
    }

    // Step 3: Render to bitmaps (now floor graphics are set correctly!)
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    if (needs_render || !bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
      room.RenderRoomGraphics();  // Includes RenderObjectsToBackground()
                                  // internally
      LOG_DEBUG("[DungeonEditorV2]", "Rendered room %d to bitmaps", room_id);
    }
  }

  // Room ID moved to card title - just show load status now
  if (room.IsLoaded()) {
    ImGui::TextColored(theme.text_success_green, ICON_MD_CHECK " Loaded");
  } else {
    ImGui::TextColored(theme.text_error_red,
                       ICON_MD_PENDING " Not Loaded");
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Objects: %zu", room.GetTileObjects().size());

  ImGui::Separator();

  // Canvas - fully delegated to DungeonCanvasViewer
  // DungeonCanvasViewer has DrawDungeonCanvas() method
  canvas_viewer_.DrawDungeonCanvas(room_id);
}

void DungeonEditorV2::OnRoomSelected(int room_id) {
  current_room_id_ = room_id;

  // Update editor system with new room
  if (dungeon_editor_system_) {
    dungeon_editor_system_->SetExternalRoom(&rooms_[room_id]);
  }

  // Check if already open
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == room_id) {
      // Focus the existing room card
      FocusRoom(room_id);
      return;
    }
  }

  // Add new room to be opened as a card
  active_rooms_.push_back(room_id);
  room_selector_.set_active_rooms(active_rooms_);

  // Register with PanelManager so it appears in Activity Bar
  if (dependencies_.panel_manager) {
    std::string room_name;
    if (room_id >= 0 &&
        static_cast<size_t>(room_id) < std::size(zelda3::kRoomNames)) {
      room_name = absl::StrFormat("[%03X] %s", room_id,
                                  zelda3::kRoomNames[room_id].data());
    } else {
      room_name = absl::StrFormat("Room %03X", room_id);
    }

    // Use base ID - RegisterPanel handles session prefixing internally
    std::string base_card_id = absl::StrFormat("dungeon.room_%d", room_id);
    
    dependencies_.panel_manager->RegisterPanel(
        {.card_id = base_card_id,
         .display_name = room_name,
         .window_title = ICON_MD_GRID_ON " " + room_name,
         .icon = ICON_MD_GRID_ON,
         .category = "Dungeon",
         .shortcut_hint = "",
         .visibility_flag = nullptr,  // PanelManager creates this
         .priority = 200 + room_id});  // After static panels
    
    // Show the panel immediately - this sets visibility to true
    dependencies_.panel_manager->ShowPanel(base_card_id);
    
    // NOT auto-pinned - user must explicitly pin to persist across editors
    // Unpinned resource panels close when switching to another editor
  }

  // Update palette based on room
  if (room_id >= 0 && room_id < (int)rooms_.size()) {
    auto& room = rooms_[room_id];
    // Ensure room header is loaded to get palette ID
    if (!room.IsLoaded()) {
      // Load just enough to get palette if possible, or trigger load
      // For now, we rely on room_loader to be fast enough or cached
      room_loader_.LoadRoom(room_id, room);
    }

    if (room.IsLoaded()) {
      current_palette_id_ = room.palette;

      // Update palette editor
      palette_editor_.SetCurrentPaletteId(current_palette_id_);

      // Update canvas viewer
      canvas_viewer_.SetCurrentPaletteId(current_palette_id_);

      // Update current palette group
      auto dungeon_main_pal_group = rom_->palette_group().dungeon_main;
      if (current_palette_id_ < (int)dungeon_main_pal_group.size()) {
        current_palette_ = dungeon_main_pal_group[current_palette_id_];
        // Propagate to canvas viewer
        canvas_viewer_.SetCurrentPaletteGroup(
            gfx::CreatePaletteGroupFromLargePalette(current_palette_).value());
      }
    }
  }
}

void DungeonEditorV2::OnEntranceSelected(int entrance_id) {
  if (entrance_id < 0 || entrance_id >= static_cast<int>(entrances_.size())) {
    return;
  }

  // Get the room ID associated with this entrance
  int room_id = entrances_[entrance_id].room_;

  // Open and focus the room
  OnRoomSelected(room_id);
}

void DungeonEditorV2::add_room(int room_id) {
  OnRoomSelected(room_id);
}

void DungeonEditorV2::FocusRoom(int room_id) {
  // Focus the room card if it exists
  auto it = room_cards_.find(room_id);
  if (it != room_cards_.end()) {
    it->second->Focus();
  }
}

void DungeonEditorV2::SelectObject(int obj_id) {
  if (object_editor_card_) {
    // Ensure object editor is visible
    show_object_editor_ = true;
    object_editor_card_->SelectObject(obj_id);
  }
}

void DungeonEditorV2::SetAgentMode(bool enabled) {
  if (enabled) {
    // Open key cards for the agent
    show_room_selector_ = true;
    show_object_editor_ = true;
    show_room_graphics_ = true;

    // Optimize layout in sub-components
    if (object_editor_card_) {
      object_editor_card_->SetAgentOptimizedLayout(true);
    }
  }
}

// =============================================================================
// Phase 4 Complete: Static panel drawing methods removed
// All static panels (RoomsList, Entrances, RoomMatrix, RoomGraphics, Debug)
// are now handled by EditorPanel implementations and drawn via
// PanelManager::DrawAllVisiblePanels()
// =============================================================================

void DungeonEditorV2::ProcessDeferredTextures() {
  // Process queued texture commands via Arena's deferred system
  // This is critical for ensuring textures are actually created and updated
  gfx::Arena::Get().ProcessTextureQueue(renderer_);
}

void DungeonEditorV2::HandleObjectPlaced(const zelda3::RoomObject& obj) {
  // Validate current room context
  if (current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_.size())) {
    LOG_ERROR("DungeonEditorV2", "Cannot place object: Invalid room ID %d",
              current_room_id_);
    return;
  }

  auto& room = rooms_[current_room_id_];

  // Log the placement for debugging
  LOG_INFO("DungeonEditorV2",
           "Placing object ID=0x%02X at position (%d,%d) in room %03X", obj.id_,
           obj.x_, obj.y_, current_room_id_);

  // Object is already added to room by PlaceObjectAtPosition in
  // DungeonObjectInteraction, so we just need to trigger re-render
  room.RenderRoomGraphics();

  LOG_DEBUG("DungeonEditorV2", "Object placed and room re-rendered successfully");
}

absl::Status DungeonEditorV2::Cut() {
  canvas_viewer_.object_interaction().HandleCopySelected();
  canvas_viewer_.object_interaction().HandleDeleteSelected();
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Copy() {
  canvas_viewer_.object_interaction().HandleCopySelected();
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Paste() {
  canvas_viewer_.object_interaction().HandlePasteObjects();
  return absl::OkStatus();
}

void DungeonEditorV2::PushUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  undo_history_[room_id].push_back(rooms_[room_id].GetTileObjects());
  ClearRedo(room_id);
}

absl::Status DungeonEditorV2::RestoreFromSnapshot(
    int room_id, std::vector<zelda3::RoomObject> snapshot) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  auto& room = rooms_[room_id];
  room.GetTileObjects() = std::move(snapshot);
  room.RenderRoomGraphics();
  return absl::OkStatus();
}

void DungeonEditorV2::ClearRedo(int room_id) {
  redo_history_[room_id].clear();
}

}  // namespace yaze::editor
