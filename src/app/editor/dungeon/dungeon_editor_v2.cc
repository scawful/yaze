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
#include "app/editor/dungeon/panels/item_editor_panel.h"
#include "app/editor/dungeon/panels/minecart_track_editor_panel.h"
#include "app/editor/dungeon/panels/object_editor_panel.h"
#include "app/editor/dungeon/panels/sprite_editor_panel.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/panel_manager.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/icons.h"
#include "core/features.h"
#include "core/project.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

DungeonEditorV2::~DungeonEditorV2() {
  // Clear viewer references in panels BEFORE room_viewers_ is destroyed.
  // Panels are owned by PanelManager and outlive this editor, so they need
  // to have their viewer pointers cleared to prevent dangling pointer access.
  if (object_editor_panel_) {
    object_editor_panel_->SetCanvasViewer(nullptr);
  }
  if (sprite_editor_panel_) {
    sprite_editor_panel_->SetCanvasViewer(nullptr);
  }
  if (item_editor_panel_) {
    item_editor_panel_->SetCanvasViewer(nullptr);
  }
}

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

  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;

  // Register panels with PanelManager (no boolean flags - visibility is
  // managed entirely by PanelManager::ShowPanel/HidePanel/IsPanelVisible)
  panel_manager->RegisterPanel(
      {.card_id = kRoomSelectorId,
       .display_name = "Room List",
       .window_title = " Room List",
       .icon = ICON_MD_LIST,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+R",
       .visibility_flag = nullptr,
       .priority = 20,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to browse dungeon rooms"});

  panel_manager->RegisterPanel(
      {.card_id = kEntranceListId,
       .display_name = "Entrance List",
       .window_title = " Entrance List",
       .icon = ICON_MD_DOOR_FRONT,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+E",
       .visibility_flag = nullptr,
       .priority = 25,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to browse dungeon entrances"});

  panel_manager->RegisterPanel(
      {.card_id = "dungeon.entrance_properties",
       .display_name = "Entrance Properties",
       .window_title = " Entrance Properties",
       .icon = ICON_MD_TUNE,
       .category = "Dungeon",
       .shortcut_hint = "",
       .visibility_flag = nullptr,
       .priority = 26,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit entrance properties"});

  panel_manager->RegisterPanel(
      {.card_id = kRoomMatrixId,
       .display_name = "Room Matrix",
       .window_title = " Room Matrix",
       .icon = ICON_MD_GRID_VIEW,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+M",
       .visibility_flag = nullptr,
       .priority = 30,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view the room matrix"});

  panel_manager->RegisterPanel(
      {.card_id = kRoomGraphicsId,
       .display_name = "Room Graphics",
       .window_title = " Room Graphics",
       .icon = ICON_MD_IMAGE,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+G",
       .visibility_flag = nullptr,
       .priority = 50,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view room graphics"});

  panel_manager->RegisterPanel(
      {.card_id = kPaletteEditorId,
       .display_name = "Palette Editor",
       .window_title = " Palette Editor",
       .icon = ICON_MD_PALETTE,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+P",
       .visibility_flag = nullptr,
       .priority = 70,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit dungeon palettes"});

  // Show default panels on startup
  panel_manager->ShowPanel(kRoomSelectorId);
  panel_manager->ShowPanel(kRoomMatrixId);

  // Register EditorPanel instances
  panel_manager->RegisterEditorPanel(std::make_unique<DungeonRoomSelectorPanel>(
      &room_selector_, [this](int room_id) { OnRoomSelected(room_id); }));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonEntranceListPanel>(
      &room_selector_,
      [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonRoomMatrixPanel>(
      &current_room_id_, &active_rooms_,
      [this](int room_id) { OnRoomSelected(room_id); },
      [this](int old_room, int new_room) {
        SwapRoomInPanel(old_room, new_room);
      },
      &rooms_));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonEntrancesPanel>(
      &entrances_, &current_entrance_id_,
      [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  // Note: DungeonRoomGraphicsPanel and DungeonPaletteEditorPanel are registered
  // in Load() after their dependencies (renderer_, palette_editor_) are initialized
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
  room_selector_.SetRoomSelectedCallback(
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

  // Register panels that depend on initialized state (renderer, palette_editor_)
  if (dependencies_.panel_manager) {
    auto graphics_panel = std::make_unique<DungeonRoomGraphicsPanel>(
        &current_room_id_, &rooms_, renderer_);
    room_graphics_panel_ = graphics_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(graphics_panel));
    dependencies_.panel_manager->RegisterEditorPanel(
        std::make_unique<DungeonPaletteEditorPanel>(&palette_editor_));
  }

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
  if (dependencies_.project) {
    object_editor_panel_->object_selector().SetCustomObjectsFolder(
        dependencies_.project->custom_objects_folder);
  }

  // Register the ObjectEditorPanel directly (it inherits from EditorPanel)
  // Panel manager takes ownership
  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->RegisterEditorPanel(std::move(object_editor));

    // Register sprite and item editor panels with canvas viewer = nullptr
    // They will get the viewer reference in OnRoomSelected when a room is selected
    auto sprite_panel = std::make_unique<SpriteEditorPanel>(&current_room_id_,
                                                            &rooms_, nullptr);
    sprite_editor_panel_ = sprite_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(sprite_panel));

    auto item_panel =
        std::make_unique<ItemEditorPanel>(&current_room_id_, &rooms_, nullptr);
    item_editor_panel_ = item_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(item_panel));

    // Feature Flag: Custom Objects / Minecart Tracks
    if (core::FeatureFlags::get().kEnableCustomObjects) {
      if (!minecart_track_editor_panel_) {
        auto minecart_panel = std::make_unique<MinecartTrackEditorPanel>();
        minecart_track_editor_panel_ = minecart_panel.get();
        dependencies_.panel_manager->RegisterEditorPanel(
            std::move(minecart_panel));
      }

      if (dependencies_.project) {
        // Update project root for track editor
        if (minecart_track_editor_panel_) {
          minecart_track_editor_panel_->SetProjectRoot(
              dependencies_.project->code_folder);
          minecart_track_editor_panel_->SetRooms(&rooms_);
          minecart_track_editor_panel_->SetProject(dependencies_.project);
          minecart_track_editor_panel_->SetRoomNavigationCallback(
              [this](int room_id) { OnRoomSelected(room_id); });
        }

        // Initialize custom object manager with project-configured path
        if (!dependencies_.project->custom_objects_folder.empty()) {
          zelda3::CustomObjectManager::Get().Initialize(
              dependencies_.project->custom_objects_folder);
        }
      }
    }
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

  // Process any pending room swaps after all drawing is complete
  // This prevents ImGui state corruption from modifying collections mid-frame
  ProcessPendingSwap();

  return absl::OkStatus();
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
      return status;
    }
    status = room.SaveSprites();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room sprites: %s",
                status.message().data());
      return status;
    }
    status = room.SaveRoomHeader();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room header: %s",
                status.message().data());
      return status;
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

  auto status = zelda3::SaveAllTorches(rom_, rooms_);
  if (!status.ok()) {
    LOG_ERROR("DungeonEditorV2", "Failed to save torches: %s",
              status.message().data());
    return status;
  }

  status = zelda3::SaveAllPits(rom_);
  if (!status.ok()) {
    LOG_ERROR("DungeonEditorV2", "Failed to save pits: %s",
              status.message().data());
    return status;
  }

  status = zelda3::SaveAllBlocks(rom_);
  if (!status.ok()) {
    LOG_ERROR("DungeonEditorV2", "Failed to save blocks: %s",
              status.message().data());
    return status;
  }

  status = zelda3::SaveAllCollision(rom_);
  if (!status.ok()) {
    LOG_ERROR("DungeonEditorV2", "Failed to save collision: %s",
              status.message().data());
    return status;
  }

  status = zelda3::SaveAllChests(rom_, rooms_);
  if (!status.ok()) {
    LOG_ERROR("DungeonEditorV2", "Failed to save chests: %s",
              status.message().data());
    return status;
  }

  status = zelda3::SaveAllPotItems(rom_, rooms_);
  if (!status.ok()) {
    LOG_ERROR("DungeonEditorV2", "Failed to save pot items: %s",
              status.message().data());
    return status;
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

    // Use unified ResourceLabelProvider for room names
    std::string base_name = absl::StrFormat(
        "[%03X] %s", room_id, zelda3::GetRoomLabel(room_id).c_str());

    std::string card_name_str = absl::StrFormat(
        "%s###RoomPanel%d", MakePanelTitle(base_name).c_str(), room_id);

    if (room_cards_.find(room_id) == room_cards_.end()) {
      room_cards_[room_id] = std::make_shared<gui::PanelWindow>(
          card_name_str.c_str(), ICON_MD_GRID_ON, &open);
      room_cards_[room_id]->SetDefaultSize(620, 700);
      // Note: Room panels use default save settings to preserve docking state
    }

    auto& room_card = room_cards_[room_id];

    ImGui::SetNextWindowClass(&room_window_class_);

    // Auto-dock room panels together using a shared dock ID
    // This ensures all room windows tab together in the same dock node
    if (room_dock_id_ == 0) {
      // Create a stable dock ID on first use
      room_dock_id_ = ImGui::GetID("DungeonRoomDock");
    }
    ImGui::SetNextWindowDockID(room_dock_id_, ImGuiCond_FirstUseEver);

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
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    LOG_WARN("DungeonEditorV2", "Ignoring invalid room selection: %d", room_id);
    return;
  }
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

  // Update sprite and item editor panels with current viewer
  if (sprite_editor_panel_) {
    sprite_editor_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }
  if (item_editor_panel_) {
    item_editor_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }

  // Sync palette with current room (must happen before early return for focus changes)
  if (room_id >= 0 && room_id < (int)rooms_.size()) {
    auto& room = rooms_[room_id];
    if (!room.IsLoaded()) {
      room_loader_.LoadRoom(room_id, room);
    }

    if (room.IsLoaded()) {
      current_palette_id_ = room.palette;
      palette_editor_.SetCurrentPaletteId(current_palette_id_);

      // Update viewer and object editor palette
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
              // Sync palette to graphics panel for proper sheet coloring
              if (room_graphics_panel_) {
                room_graphics_panel_->SetCurrentPaletteGroup(
                    current_palette_group_);
              }
            }
          }
        }
      }
    }
  }

  // Check if room is already open
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == room_id) {
      // Always ensure panel is visible, even if already in active_rooms_
      if (dependencies_.panel_manager) {
        std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
        dependencies_.panel_manager->ShowPanel(card_id);
      }
      if (request_focus) {
        FocusRoom(room_id);
      }
      return;
    }
  }

  active_rooms_.push_back(room_id);
  room_selector_.set_active_rooms(active_rooms_);

  if (dependencies_.panel_manager) {
    // Use unified ResourceLabelProvider for room names
    std::string room_name = absl::StrFormat(
        "[%03X] %s", room_id, zelda3::GetRoomLabel(room_id).c_str());

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
}

void DungeonEditorV2::OnEntranceSelected(int entrance_id) {
  if (entrance_id < 0 || entrance_id >= static_cast<int>(entrances_.size())) {
    return;
  }
  int room_id = entrances_[entrance_id].room_;
  OnRoomSelected(room_id);
}

void DungeonEditorV2::add_room(int room_id) {
  OnRoomSelected(room_id);
}

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

void DungeonEditorV2::OpenGraphicsEditorForObject(
    int room_id, const zelda3::RoomObject& object) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    LOG_WARN("DungeonEditorV2",
             "Edit Graphics ignored (invalid room id %d)", room_id);
    return;
  }

  auto* editor_manager =
      static_cast<EditorManager*>(dependencies_.custom_data);
  if (!editor_manager) {
    LOG_WARN("DungeonEditorV2",
             "Edit Graphics ignored (editor manager unavailable)");
    return;
  }

  auto& room = rooms_[room_id];
  room.LoadRoomGraphics(room.blockset);

  uint16_t sheet_id = 0;
  uint16_t tile_index = 0;
  bool resolved_sheet = false;
  if (auto tiles_or = object.GetTiles(); tiles_or.ok() &&
      !tiles_or.value().empty()) {
    const uint16_t tile_id = tiles_or.value().front().id_;
    const size_t block_index = static_cast<size_t>(tile_id / 64);
    const auto blocks = room.blocks();
    if (block_index < blocks.size()) {
      sheet_id = blocks[block_index];
      resolved_sheet = true;
    }
    const int tiles_per_row = gfx::kTilesheetWidth / 8;
    const int tiles_per_col = gfx::kTilesheetHeight / 8;
    const int tiles_per_sheet = tiles_per_row * tiles_per_col;
    if (tiles_per_sheet > 0) {
      tile_index = static_cast<uint16_t>(tile_id % tiles_per_sheet);
    }
  }

  editor_manager->SwitchToEditor(EditorType::kGraphics, true);
  if (auto* editor_set = editor_manager->GetCurrentEditorSet()) {
    if (auto* graphics = editor_set->GetGraphicsEditor()) {
      if (resolved_sheet) {
        graphics->SelectSheet(sheet_id);
        graphics->HighlightTile(
            sheet_id, tile_index,
            absl::StrFormat("Object 0x%02X", object.id_));
      }
    }
  }

  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->ShowAllPanelsInCategory("Graphics");
  }
}

void DungeonEditorV2::SwapRoomInPanel(int old_room_id, int new_room_id) {
  // Defer the swap until after the current frame's draw phase completes
  // This prevents modifying data structures while ImGui is still using them
  if (new_room_id < 0 || new_room_id >= static_cast<int>(rooms_.size())) {
    return;
  }
  pending_swap_.old_room_id = old_room_id;
  pending_swap_.new_room_id = new_room_id;
  pending_swap_.pending = true;
}

void DungeonEditorV2::ProcessPendingSwap() {
  if (!pending_swap_.pending) {
    return;
  }

  int old_room_id = pending_swap_.old_room_id;
  int new_room_id = pending_swap_.new_room_id;
  pending_swap_.pending = false;

  // Find the position of old_room in active_rooms_
  int swap_index = -1;
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == old_room_id) {
      swap_index = i;
      break;
    }
  }

  if (swap_index < 0) {
    // Old room not found in active rooms, just select the new one
    OnRoomSelected(new_room_id);
    return;
  }

  // Replace old room with new room in active_rooms_
  active_rooms_[swap_index] = new_room_id;
  room_selector_.set_active_rooms(active_rooms_);

  // Unregister old panel
  if (dependencies_.panel_manager) {
    std::string old_card_id = absl::StrFormat("dungeon.room_%d", old_room_id);
    dependencies_.panel_manager->UnregisterPanel(old_card_id);
  }

  // Clean up old room's card and viewer
  room_cards_.erase(old_room_id);
  room_viewers_.erase(old_room_id);

  // Register new panel
  if (dependencies_.panel_manager) {
    // Use unified ResourceLabelProvider for room names
    std::string new_room_name = absl::StrFormat(
        "[%03X] %s", new_room_id, zelda3::GetRoomLabel(new_room_id).c_str());

    std::string new_card_id = absl::StrFormat("dungeon.room_%d", new_room_id);

    dependencies_.panel_manager->RegisterPanel(
        {.card_id = new_card_id,
         .display_name = new_room_name,
         .window_title = ICON_MD_GRID_ON " " + new_room_name,
         .icon = ICON_MD_GRID_ON,
         .category = "Dungeon",
         .shortcut_hint = "",
         .visibility_flag = nullptr,
         .priority = 200 + new_room_id});

    dependencies_.panel_manager->ShowPanel(new_card_id);
  }

  // Update current selection
  OnRoomSelected(new_room_id, /*request_focus=*/false);
}

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
            rooms_[room_id].MarkObjectsDirty();
            rooms_[room_id].RenderRoomGraphics();
          }
        });

    viewer->object_interaction().SetObjectPlacedCallback(
        [this](const zelda3::RoomObject& obj) { HandleObjectPlaced(obj); });

    if (dungeon_editor_system_) {
      viewer->SetEditorSystem(dungeon_editor_system_.get());
    }
    viewer->SetRoomNavigationCallback([this](int target_room) {
      if (target_room >= 0 && target_room < static_cast<int>(rooms_.size())) {
        OnRoomSelected(target_room);
      }
    });
    // Swap callback swaps the room in the current panel instead of opening new
    viewer->SetRoomSwapCallback([this](int old_room, int new_room) {
      SwapRoomInPanel(old_room, new_room);
    });
    viewer->SetShowObjectPanelCallback([this]() { ShowPanel(kObjectToolsId); });
    viewer->SetShowSpritePanelCallback(
        [this]() { ShowPanel("dungeon.sprite_editor"); });
    viewer->SetShowItemPanelCallback(
        [this]() { ShowPanel("dungeon.item_editor"); });
    viewer->SetShowRoomListCallback([this]() { ShowPanel(kRoomSelectorId); });
    viewer->SetShowRoomMatrixCallback([this]() { ShowPanel(kRoomMatrixId); });
    viewer->SetShowEntranceListCallback(
        [this]() { ShowPanel(kEntranceListId); });
    viewer->SetShowRoomGraphicsCallback(
        [this]() { ShowPanel(kRoomGraphicsId); });
    viewer->SetEditGraphicsCallback(
        [this](int target_room_id, const zelda3::RoomObject& object) {
          OpenGraphicsEditorForObject(target_room_id, object);
        });
    viewer->SetMinecartTrackPanel(minecart_track_editor_panel_);
    viewer->SetProject(dependencies_.project);

    room_viewers_[room_id] = std::move(viewer);
    return room_viewers_[room_id].get();
  }
  return it->second.get();
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
