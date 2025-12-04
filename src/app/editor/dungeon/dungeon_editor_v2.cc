// Related header
#include "dungeon_editor_v2.h"

// C system headers
#include <cstdio>

// C++ standard library headers
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Third-party library headers
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/panels/dungeon_entrance_list_panel.h"
#include "app/editor/dungeon/panels/dungeon_entrances_panel.h"
#include "app/editor/dungeon/panels/dungeon_palette_editor_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_graphics_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_matrix_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_selector_panel.h"
#include "app/editor/dungeon/panels/object_editor_panel.h"
#include "app/editor/system/panel_manager.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/icons.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

void DungeonEditorV2::Initialize(gfx::IRenderer* renderer, Rom* rom) {
  renderer_ = renderer;
  rom_ = rom;

  // Propagate ROM to all rooms
  if (rom_) {
    for (auto& room : rooms_) {
      room.SetRom(rom_);
    }
  }

  // Setup docking class for room windows
  room_window_class_.DockingAllowUnclassed = true;
  room_window_class_.DockingAlwaysTabBar = true;

  if (!dependencies_.panel_manager) return;
  auto* panel_manager = dependencies_.panel_manager;

  // Register panels with PanelManager (no boolean flags - visibility is
  // managed entirely by PanelManager::ShowPanel/HidePanel/IsPanelVisible)
  panel_manager->RegisterPanel(
      {.card_id = kControlPanelId,
       .display_name = "Dungeon Controls",
       .window_title = " Dungeon Controls",
       .icon = ICON_MD_CASTLE,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+D",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to access dungeon controls",
       .priority = 10});

  panel_manager->RegisterPanel(
      {.card_id = kRoomSelectorId,
       .display_name = "Room List",
       .window_title = " Room List",
       .icon = ICON_MD_LIST,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+R",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to browse dungeon rooms",
       .priority = 20});

  panel_manager->RegisterPanel(
      {.card_id = kEntranceListId,
       .display_name = "Entrance List",
       .window_title = " Entrance List",
       .icon = ICON_MD_DOOR_FRONT,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+E",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to browse dungeon entrances",
       .priority = 25});

  panel_manager->RegisterPanel(
      {.card_id = "dungeon.entrance_properties",
       .display_name = "Entrance Properties",
       .window_title = " Entrance Properties",
       .icon = ICON_MD_TUNE,
       .category = "Dungeon",
       .shortcut_hint = "",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit entrance properties",
       .priority = 26});

  panel_manager->RegisterPanel(
      {.card_id = kRoomMatrixId,
       .display_name = "Room Matrix",
       .window_title = " Room Matrix",
       .icon = ICON_MD_GRID_VIEW,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+M",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view the room matrix",
       .priority = 30});

  panel_manager->RegisterPanel(
      {.card_id = kRoomGraphicsId,
       .display_name = "Room Graphics",
       .window_title = " Room Graphics",
       .icon = ICON_MD_IMAGE,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+G",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view room graphics",
       .priority = 50});

  panel_manager->RegisterPanel(
      {.card_id = kPaletteEditorId,
       .display_name = "Palette Editor",
       .window_title = " Palette Editor",
       .icon = ICON_MD_PALETTE,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+P",
       .visibility_flag = nullptr,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit dungeon palettes",
       .priority = 70});

  // Show default panels on startup
  panel_manager->ShowPanel(kControlPanelId);
  panel_manager->ShowPanel(kRoomSelectorId);

  // Register EditorPanel instances
  panel_manager->RegisterEditorPanel(std::make_unique<DungeonRoomSelectorPanel>(
      &room_selector_, [this](int room_id) { OnRoomSelected(room_id); }));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonEntranceListPanel>(
      &room_selector_,
      [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonRoomMatrixPanel>(
      &current_room_id_, &active_rooms_,
      [this](int room_id) { OnRoomSelected(room_id); }));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonEntrancesPanel>(
      &entrances_, &current_entrance_id_,
      [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonRoomGraphicsPanel>(&current_room_id_, &rooms_));

  panel_manager->RegisterEditorPanel(
      std::make_unique<DungeonPaletteEditorPanel>(&palette_editor_));
}

void DungeonEditorV2::Initialize() {}

absl::Status DungeonEditorV2::Load() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Load object dimension table for accurate hit-testing
  auto& dim_table = zelda3::ObjectDimensionTable::Get();
  if (!dim_table.IsLoaded()) {
    RETURN_IF_ERROR(dim_table.LoadFromRom(rom_));
  }

  RETURN_IF_ERROR(room_loader_.LoadRoomEntrances(entrances_));

  if (!game_data()) {
    return absl::FailedPreconditionError("GameData not available");
  }
  auto dungeon_main_pal_group = game_data()->palette_groups.dungeon_main;
  current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
  ASSIGN_OR_RETURN(current_palette_group_,
                   gfx::CreatePaletteGroupFromLargePalette(current_palette_));

  room_selector_.set_rooms(&rooms_);
  room_selector_.set_entrances(&entrances_);
  room_selector_.set_active_rooms(active_rooms_);
  room_selector_.set_room_selected_callback(
      [this](int room_id) { OnRoomSelected(room_id); });

  // Canvas viewers are lazily created in GetViewerForRoom

  if (!render_service_) {
    render_service_ =
        std::make_unique<emu::render::EmulatorRenderService>(rom_);
    auto status = render_service_->Initialize();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to initialize render service: %s",
                status.message().data());
    }
  }

  if (game_data()) {
    gfx::PaletteManager::Get().Initialize(game_data());
  } else {
    gfx::PaletteManager::Get().Initialize(rom_);
  }

  palette_editor_.Initialize(game_data());

  dungeon_editor_system_ = std::make_unique<zelda3::DungeonEditorSystem>(rom_);
  (void)dungeon_editor_system_->Initialize();
  dungeon_editor_system_->SetCurrentRoom(current_room_id_);

  // Initialize unified object editor panel
  // Note: Initially passing nullptr for viewer, will be set on selection
  auto object_editor = std::make_unique<ObjectEditorPanel>(
      renderer_, rom_, nullptr, dungeon_editor_system_->GetObjectEditor());

  // Wire up object change callback to trigger room re-rendering
  dungeon_editor_system_->GetObjectEditor()->SetObjectChangedCallback(
      [this](size_t /*object_index*/, const zelda3::RoomObject& /*object*/) {
        if (current_room_id_ >= 0 && current_room_id_ < (int)rooms_.size()) {
          rooms_[current_room_id_].RenderRoomGraphics();
        }
      });

  // Set rooms and initial palette group for correct preview rendering
  object_editor->SetRooms(&rooms_);
  object_editor->SetCurrentPaletteGroup(current_palette_group_);

  // Keep raw pointer for later access
  object_editor_panel_ = object_editor.get();

  // Propagate game_data to the object editor panel if available
  if (game_data()) {
    object_editor_panel_->SetGameData(game_data());
  }

  // Register the ObjectEditorPanel directly (it inherits from EditorPanel)
  // Panel manager takes ownership
  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->RegisterEditorPanel(std::move(object_editor));
  } else {
    owned_object_editor_panel_ = std::move(object_editor);
  }

  palette_editor_.SetOnPaletteChanged([this](int /*palette_id*/) {
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
  if (room_window_class_.ClassId == 0) {
    room_window_class_.ClassId = ImGui::GetID("DungeonRoomClass");
  }

  if (!is_loaded_) {
    gui::PanelWindow loading_card("Dungeon Editor Loading", ICON_MD_CASTLE);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::TextColored(theme.text_secondary_gray, "Loading dungeon data...");
      ImGui::TextWrapped(
          "Independent editor cards will appear once ROM data is loaded.");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  DrawRoomPanels();

  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    // Delegate delete to current room viewer
    if (auto* viewer = GetViewerForRoom(current_room_id_)) {
      viewer->DeleteSelectedObjects();
    }
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

  for (auto& room : rooms_) {
    auto status = room.SaveObjects();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room objects: %s",
                status.message().data());
    }

    if (dungeon_editor_system_) {
      auto sys_status = dungeon_editor_system_->SaveRoom(room.id());
      if (!sys_status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Failed to save room system data: %s",
                  sys_status.message().data());
      }
    }
  }

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

void DungeonEditorV2::DrawRoomPanels() {
  for (int i = 0; i < active_rooms_.Size; i++) {
    int room_id = active_rooms_[i];
    std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
    bool panel_visible = true;
    if (dependencies_.panel_manager) {
      panel_visible = dependencies_.panel_manager->IsPanelVisible(card_id);
    }

    if (!panel_visible) {
      dependencies_.panel_manager->UnregisterPanel(card_id);
      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      // Clean up viewer
      room_viewers_.erase(room_id);
      i--;
      continue;
    }

    bool is_pinned = dependencies_.panel_manager &&
                     dependencies_.panel_manager->IsPanelPinned(card_id);
    std::string active_category =
        dependencies_.panel_manager
            ? dependencies_.panel_manager->GetActiveCategory()
            : "";

    if (active_category != "Dungeon" && !is_pinned) {
      continue;
    }

    bool open = true;

    std::string base_name;
    if (room_id >= 0 &&
        static_cast<size_t>(room_id) < std::size(zelda3::kRoomNames)) {
      base_name = absl::StrFormat("[%03X] %s", room_id,
                                  zelda3::kRoomNames[room_id].data());
    } else {
      base_name = absl::StrFormat("Room %03X", room_id);
    }

    std::string card_name_str = absl::StrFormat(
        "%s###RoomPanel%d", MakePanelTitle(base_name).c_str(), room_id);

    if (room_cards_.find(room_id) == room_cards_.end()) {
      room_cards_[room_id] = std::make_shared<gui::PanelWindow>(
          card_name_str.c_str(), ICON_MD_GRID_ON, &open);
      room_cards_[room_id]->SetDefaultSize(700, 600);

      if (active_rooms_.Size == 1) {
        room_cards_[room_id]->SetPosition(gui::PanelWindow::Position::Floating);
      }
    }

    auto& room_card = room_cards_[room_id];

    ImGui::SetNextWindowClass(&room_window_class_);

    if (room_card->Begin(&open)) {
      // Ensure focused room updates selection context
      if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        OnRoomSelected(room_id, /*request_focus=*/false);
      }
      DrawRoomTab(room_id);
    }
    room_card->End();

    if (!open) {
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }

      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      room_viewers_.erase(room_id);
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

  if (!room.IsLoaded()) {
    auto status = room_loader_.LoadRoom(room_id, room);
    if (!status.ok()) {
      ImGui::TextColored(theme.text_error_red, "Failed to load room: %s",
                         status.message().data());
      return;
    }

    if (dungeon_editor_system_) {
      auto sys_status = dungeon_editor_system_->ReloadRoom(room_id);
      if (!sys_status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Failed to load system data: %s",
                  sys_status.message().data());
      }
    }
  }

  if (room.IsLoaded()) {
    bool needs_render = false;

    // Chronological Step 1: Load Room Data from ROM
    // This reads the 14-byte room header (blockset, palette, effect, tags)
    // Reference: kRoomHeaderPointer (0xB5DD)
    if (room.blocks().empty()) {
      room.LoadRoomGraphics(room.blockset);
      needs_render = true;
      LOG_DEBUG("[DungeonEditorV2]", "Loaded room %d graphics from ROM",
                room_id);
    }

    // Chronological Step 2: Load Objects from ROM
    // This reads the variable-length object stream (subtype 1, 2, 3 objects)
    // Reference: kRoomObjectPointer (0x874C)
    // CRITICAL: This step decodes floor1/floor2 bytes which dictate the floor
    // pattern
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
      needs_render = true;
      LOG_DEBUG("[DungeonEditorV2]", "Loaded room %d objects from ROM",
                room_id);
    }

    // Chronological Step 3: Render Graphics to Bitmaps
    // This executes the draw routines (bank_01.asm logic) to populate BG1/BG2
    // buffers Sequence:
    // 1. Draw Floor (from floor1/floor2)
    // 2. Draw Layout (walls/floors from object list)
    // 3. Draw Objects (subtypes 1, 2, 3)
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    if (needs_render || !bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
      room.RenderRoomGraphics();
      LOG_DEBUG("[DungeonEditorV2]", "Rendered room %d to bitmaps", room_id);
    }
  }

  if (room.IsLoaded()) {
    ImGui::TextColored(theme.text_success_green, ICON_MD_CHECK " Loaded");
  } else {
    ImGui::TextColored(theme.text_error_red, ICON_MD_PENDING " Not Loaded");
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Objects: %zu", room.GetTileObjects().size());

  ImGui::Separator();

  // Use per-room viewer
  if (auto* viewer = GetViewerForRoom(room_id)) {
    viewer->DrawDungeonCanvas(room_id);
  }
}

void DungeonEditorV2::OnRoomSelected(int room_id, bool request_focus) {
  current_room_id_ = room_id;

  if (dungeon_editor_system_) {
    dungeon_editor_system_->SetExternalRoom(&rooms_[room_id]);
  }

  // Update object editor card with current viewer
  if (object_editor_panel_) {
    object_editor_panel_->SetCurrentRoom(room_id);
    // IMPORTANT: Update the viewer reference!
    object_editor_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }

  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == room_id) {
      if (request_focus) {
        FocusRoom(room_id);
      }
      return;
    }
  }

  active_rooms_.push_back(room_id);
  room_selector_.set_active_rooms(active_rooms_);

  if (dependencies_.panel_manager) {
    std::string room_name;
    if (room_id >= 0 &&
        static_cast<size_t>(room_id) < std::size(zelda3::kRoomNames)) {
      room_name = absl::StrFormat("[%03X] %s", room_id,
                                  zelda3::kRoomNames[room_id].data());
    } else {
      room_name = absl::StrFormat("Room %03X", room_id);
    }

    std::string base_card_id = absl::StrFormat("dungeon.room_%d", room_id);

    dependencies_.panel_manager->RegisterPanel(
        {.card_id = base_card_id,
         .display_name = room_name,
         .window_title = ICON_MD_GRID_ON " " + room_name,
         .icon = ICON_MD_GRID_ON,
         .category = "Dungeon",
         .shortcut_hint = "",
         .visibility_flag = nullptr,
         .priority = 200 + room_id});

    dependencies_.panel_manager->ShowPanel(base_card_id);
  }

  if (room_id >= 0 && room_id < (int)rooms_.size()) {
    auto& room = rooms_[room_id];
    if (!room.IsLoaded()) {
      room_loader_.LoadRoom(room_id, room);
    }

    if (room.IsLoaded()) {
      current_palette_id_ = room.palette;
      palette_editor_.SetCurrentPaletteId(current_palette_id_);

      // Update viewer palette
      if (auto* viewer = GetViewerForRoom(room_id)) {
        viewer->SetCurrentPaletteId(current_palette_id_);

        if (game_data()) {
          auto dungeon_main_pal_group =
              game_data()->palette_groups.dungeon_main;
          if (current_palette_id_ < (int)dungeon_main_pal_group.size()) {
            current_palette_ = dungeon_main_pal_group[current_palette_id_];
            auto result =
                gfx::CreatePaletteGroupFromLargePalette(current_palette_);
            if (result.ok()) {
              current_palette_group_ = result.value();
              viewer->SetCurrentPaletteGroup(current_palette_group_);
              if (object_editor_panel_) {
                object_editor_panel_->SetCurrentPaletteGroup(
                    current_palette_group_);
              }
            }
          }
        }
      }
    }
  }
}

void DungeonEditorV2::OnEntranceSelected(int entrance_id) {
  if (entrance_id < 0 || entrance_id >= static_cast<int>(entrances_.size())) {
    return;
  }
  int room_id = entrances_[entrance_id].room_;
  OnRoomSelected(room_id);
}

void DungeonEditorV2::add_room(int room_id) { OnRoomSelected(room_id); }

void DungeonEditorV2::FocusRoom(int room_id) {
  auto it = room_cards_.find(room_id);
  if (it != room_cards_.end()) {
    it->second->Focus();
  }
}

void DungeonEditorV2::SelectObject(int obj_id) {
  if (object_editor_panel_) {
    ShowPanel(kObjectToolsId);
    object_editor_panel_->SelectObject(obj_id);
  }
}

void DungeonEditorV2::SetAgentMode(bool enabled) {
  if (enabled && dependencies_.panel_manager) {
    ShowPanel(kRoomSelectorId);
    ShowPanel(kObjectToolsId);
    ShowPanel(kRoomGraphicsId);
    if (object_editor_panel_) {
      object_editor_panel_->SetAgentOptimizedLayout(true);
    }
  }
}

void DungeonEditorV2::ProcessDeferredTextures() {
  gfx::Arena::Get().ProcessTextureQueue(renderer_);
}

void DungeonEditorV2::HandleObjectPlaced(const zelda3::RoomObject& obj) {
  if (current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_.size())) {
    LOG_ERROR("DungeonEditorV2", "Cannot place object: Invalid room ID %d",
              current_room_id_);
    return;
  }

  auto& room = rooms_[current_room_id_];

  LOG_INFO("DungeonEditorV2",
           "Placing object ID=0x%02X at position (%d,%d) in room %03X", obj.id_,
           obj.x_, obj.y_, current_room_id_);

  room.RenderRoomGraphics();
  LOG_DEBUG("DungeonEditorV2",
            "Object placed and room re-rendered successfully");
}

absl::Status DungeonEditorV2::Cut() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandleCopySelected();
    viewer->object_interaction().HandleDeleteSelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Copy() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandleCopySelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Paste() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandlePasteObjects();
  }
  return absl::OkStatus();
}

void DungeonEditorV2::PushUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) return;

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

void DungeonEditorV2::ClearRedo(int room_id) { redo_history_[room_id].clear(); }

DungeonCanvasViewer* DungeonEditorV2::GetViewerForRoom(int room_id) {
  auto it = room_viewers_.find(room_id);
  if (it == room_viewers_.end()) {
    auto viewer = std::make_unique<DungeonCanvasViewer>(rom_);
    viewer->SetRooms(&rooms_);
    viewer->SetRenderer(renderer_);
    viewer->SetCurrentPaletteGroup(current_palette_group_);
    viewer->SetCurrentPaletteId(current_palette_id_);
    viewer->SetGameData(game_data_);

    viewer->object_interaction().SetMutationHook(
        [this, room_id]() { PushUndoSnapshot(room_id); });

    viewer->object_interaction().SetCacheInvalidationCallback(
        [this, room_id]() {
          if (room_id >= 0 && room_id < static_cast<int>(rooms_.size())) {
            rooms_[room_id].RenderRoomGraphics();
          }
        });

    viewer->object_interaction().SetObjectPlacedCallback(
        [this](const zelda3::RoomObject& obj) { HandleObjectPlaced(obj); });

    if (dungeon_editor_system_) {
      viewer->SetEditorSystem(dungeon_editor_system_.get());
    }

    room_viewers_[room_id] = std::move(viewer);
    return room_viewers_[room_id].get();
  }
  return it->second.get();
}

}  // namespace yaze::editor
